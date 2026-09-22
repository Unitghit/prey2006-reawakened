// Included by FileSystem.cpp only. Worker owns all archive handles and buffers.
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

static idCVar com_assetPreload("com_assetPreload", "0", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_BOOL,
	"Experimental per-map learned archive prefetch; takes effect on map/save load");

namespace {
static const char *preloadManifestVersion = "# prey-prefetch-v2 gameplay-only";
struct preloadJob_t {
	std::string name, archive, fullPath;
	ZPOS64_T offset;
	int length;
	ID_TIME_T timestamp;
};
struct preloadData_t {
	preloadJob_t source;
	std::shared_ptr<std::vector<char> > bytes;
};
struct preloadState_t {
	std::mutex mutex;
	std::condition_variable wake;
	std::atomic<bool> stop;
	std::thread worker;
	std::deque<preloadJob_t> jobs;
	std::map<std::string, preloadData_t> ready;
	// The following fields are exclusively main-thread owned.
	std::deque<std::string> pending;
	std::set<std::string> observed;
	std::vector<std::string> order;
	std::string manifest;
	bool active = false;
	size_t reserved = 0;
	unsigned hits = 0;
	std::atomic<unsigned> completed, failed;
	preloadState_t() : stop(false), completed(0), failed(0) {}
	void Join() {
		stop = true; wake.notify_all();
		if (worker.joinable()) worker.join();
	}
	~preloadState_t() { Join(); }
} preload;

static bool PreloadPath(const char *path) {
	if (!path || !*path || strlen(path) > 240 || strstr(path,"..") || strchr(path,':') || path[0]=='/' || path[0]=='\\') return false;
	for (const char *p=path; *p; ++p) if ((unsigned char)*p<32 || *p=='"') return false;
	const char *ext = strrchr(path,'.');
	if (!ext) return false;
	const char *allowed[] = {".dds", ".tga", ".jpg", ".md5mesh", ".md5anim", ".lwo", ".ase", ".wav", ".ogg", ".gui", ".af", NULL};
	for (int i=0; allowed[i]; ++i) if (!idStr::Icmp(ext,allowed[i])) return true;
	return false;
}
static std::string PreloadKey(const char *path) {
	std::string key(path);
	for (char &c:key) { if(c=='\\') c='/'; if(c>='A' && c<='Z') c+='a'-'A'; }
	return key;
}
static void PreloadWorker() {
#if SDL_MAJOR_VERSION >= 2
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
#endif
	for (;;) {
		preloadJob_t job;
		{
			std::unique_lock<std::mutex> lock(preload.mutex);
			preload.wake.wait(lock, []{ return preload.stop || !preload.jobs.empty(); });
			if (preload.stop) return;
			job = std::move(preload.jobs.front()); preload.jobs.pop_front();
		}
		try {
			auto bytes = std::make_shared<std::vector<char> >(job.length);
			unzFile zip = unzOpen64(job.archive.c_str());
			bool ok = zip && unzSetOffset64(zip,job.offset)==UNZ_OK;
			unz_file_info64 info;
			if (ok) ok = unzGetCurrentFileInfo64(zip,&info,NULL,0,NULL,0,NULL,0)==UNZ_OK && info.uncompressed_size==(ZPOS64_T)job.length;
			bool opened = ok && unzOpenCurrentFile(zip)==UNZ_OK;
			ok = opened;
			int done=0;
			while (ok && done<job.length && !preload.stop) {
				int count = unzReadCurrentFile(zip,bytes->data()+done, Min(65536,job.length-done));
				if(count<=0) { ok=false; break; }
				done+=count;
			}
			if(opened && unzCloseCurrentFile(zip)!=UNZ_OK) ok=false;
			if(zip) unzClose(zip);
			if(preload.stop) return;
			if(ok && done==job.length) {
				std::lock_guard<std::mutex> lock(preload.mutex);
				preload.ready.emplace(job.name,preloadData_t{job,bytes});
				++preload.completed;
			} else ++preload.failed;
		} catch (...) { ++preload.failed; }
	}
}
class preloadFile_t : public idFile_Memory {
	std::shared_ptr<std::vector<char> > bytes;
	std::string fullPath;
	ID_TIME_T timestamp;
public:
	preloadFile_t(const char *name,const preloadData_t &data) : idFile_Memory(name), bytes(data.bytes), fullPath(data.source.fullPath), timestamp(data.source.timestamp) {
		SetData(bytes->data(),(int)bytes->size());
	}
	virtual const char *GetFullPath() { return fullPath.c_str(); }
	virtual ID_TIME_T Timestamp() { return timestamp; }
};
static idFile *FS_PreloadLookup(const char *path, idFile *file) {
	if(!file || !Sys_IsMainThread() || !preload.active || !com_assetPreload.GetBool() || !PreloadPath(path)) return file;
	const std::string key=PreloadKey(path);
	if(!idStr::Cmp(sessLocal.HitchState(),"gameplay") && preload.order.size()<512 && preload.observed.insert(key).second) preload.order.push_back(key);
	preloadData_t found;
	{
		std::lock_guard<std::mutex> lock(preload.mutex);
		auto it=preload.ready.find(key);
		if(it==preload.ready.end()) return file;
		found=it->second;
	}
	// Resolve through the normal search path first, preserving overrides and pure rules.
	if(found.source.fullPath!=file->GetFullPath() || found.source.length!=file->Length() || found.source.timestamp!=file->Timestamp()) return file;
	idFile *cached=new preloadFile_t(path,found);
	fileSystem->CloseFile(file);
	++preload.hits;
	return cached;
}
}

void FS_PreloadStop() {
	preload.active=false;
	preload.Join();
	if(!preload.manifest.empty() && fileSystem->IsInitialized()) {
		idFile *out=fileSystem->OpenFileWrite(preload.manifest.c_str());
		if(out) {
			out->Printf("%s\n",preloadManifestVersion);
			for(const auto &name:preload.order) out->Printf("%s\n",name.c_str());
			fileSystem->CloseFile(out);
		}
		if(cvarSystem->GetCVarBool("com_hitchTrace")) common->Printf("HITCH_PRELOAD completed=%u failed=%u hits=%u reserved_bytes=%u learned=%u\n",
			preload.completed.load(),preload.failed.load(),preload.hits,(unsigned)preload.reserved,(unsigned)preload.order.size());
	}
	preload.jobs.clear(); preload.ready.clear(); preload.pending.clear();
	preload.observed.clear(); preload.order.clear(); preload.manifest.clear();
	preload.reserved=0; preload.hits=0; preload.completed=0; preload.failed=0;
}

void FS_PreloadStart(const char *map) {
	FS_PreloadStop();
	if(!com_assetPreload.GetBool() || idAsyncNetwork::IsActive() || eventLoop->JournalLevel()!=0) return;
	if(!map || !*map || strstr(map,"..") || strchr(map,':')) return;
	preload.manifest=std::string("preload/")+map+".txt";
	idFile *in=fileSystem->OpenFileRead(preload.manifest.c_str(),false);
	if(in) {
		if(in->Length()>0 && in->Length()<=128*1024) {
			std::vector<char> text(in->Length()+1,0);
			in->Read(text.data(),in->Length());
			std::string line;
			bool firstLine=true, validVersion=false;
			for(char c:text) {
				if(c=='\n' || c==0) {
					if(firstLine) { validVersion=(line==preloadManifestVersion); firstLine=false; }
					else if(validVersion && preload.order.size()<512 && PreloadPath(line.c_str())) {
						auto key=PreloadKey(line.c_str());
						if(preload.observed.insert(key).second) { preload.pending.push_back(key); preload.order.push_back(key); }
					}
					line.clear();
				} else if(c!='\r') line+=c;
			}
		}
		fileSystem->CloseFile(in);
	}
	preload.stop=false;
	try { preload.worker=std::thread(PreloadWorker); preload.active=true; }
	catch (...) { common->Warning("Asset prefetch worker unavailable; normal loading retained"); }
	if(cvarSystem->GetCVarBool("com_hitchTrace")) common->Printf("HITCH_PRELOAD queued=%u map=%s\n",(unsigned)preload.pending.size(),map);
}

void FS_PreloadPump() {
	if(!preload.active || !com_assetPreload.GetBool()) return;
	const double start=Sys_PresentationMilliseconds();
	for(int count=0; count<2 && !preload.pending.empty() && Sys_PresentationMilliseconds()-start<1.0; ++count) {
		std::string name=preload.pending.front(); preload.pending.pop_front();
		pack_t *pack=NULL;
		idFile *file=fileSystemLocal.OpenFileReadFlags(name.c_str(),FSFLAG_SEARCH_DIRS|FSFLAG_SEARCH_PAKS,&pack,false,NULL);
		if(!file) continue;
		// Cache only immutable archives. Loose developer files retain normal live reads.
		if(pack && file->Length()>0 && file->Length()<=16*1024*1024 && preload.reserved+file->Length()<=128*1024*1024) {
			preloadJob_t job;
			job.name=name; job.archive=pack->pakFilename.c_str(); job.fullPath=file->GetFullPath();
			job.offset=static_cast<idFile_InZip*>(file)->zipFilePos;
			job.length=file->Length(); job.timestamp=file->Timestamp();
			preload.reserved+=job.length;
			{
				std::lock_guard<std::mutex> lock(preload.mutex);
				preload.jobs.push_back(std::move(job));
			}
			preload.wake.notify_one();
		}
		fileSystem->CloseFile(file);
	}
}

static void FS_PreloadStatus_f(const idCmdArgs &) {
	common->Printf("PRELOAD_STATUS active=%d pending=%u completed=%u failed=%u hits=%u reserved_bytes=%u\n",
		preload.active?1:0,(unsigned)preload.pending.size(),preload.completed.load(),preload.failed.load(),preload.hits,(unsigned)preload.reserved);
}

// Diagnostic only: compare cached bytes with a fresh normal-path read.
static void FS_PreloadVerify_f(const idCmdArgs &) {
	std::vector<std::string> names;
	{
		std::lock_guard<std::mutex> lock(preload.mutex);
		for(const auto &entry:preload.ready) names.push_back(entry.first);
	}
	unsigned passed=0, failed=0;
	char a[65536], b[65536];
	for(const auto &name:names) {
		idFile *cached=fileSystem->OpenFileRead(name.c_str(),false);
		const bool active=preload.active;
		preload.active=false;
		idFile *normal=fileSystem->OpenFileRead(name.c_str(),false);
		preload.active=active;
		bool ok=cached && normal && dynamic_cast<preloadFile_t*>(cached) && cached->Length()==normal->Length();
		int left=ok?normal->Length():0;
		while(ok && left>0) {
			int n=Min(left,(int)sizeof(a));
			ok=cached->Read(a,n)==n && normal->Read(b,n)==n && !memcmp(a,b,n);
			left-=n;
		}
		if(cached) fileSystem->CloseFile(cached);
		if(normal) fileSystem->CloseFile(normal);
		if(ok) ++passed; else ++failed;
	}
	common->Printf("PRELOAD_VERIFY passed=%u failed=%u\n",passed,failed);
}
