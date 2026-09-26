/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop

#include "snd_local.h"

#define USE_SOUND_CACHE_ALLOCATOR

#ifdef USE_SOUND_CACHE_ALLOCATOR
static idDynamicBlockAlloc<byte, 1<<20, 1<<10>	soundCacheAllocator;
#else
static idDynamicAlloc<byte, 1<<20, 1<<10>		soundCacheAllocator;
#endif

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

/*
===============================================================================

Threaded load-time OGG decoding (s_threadedDecode)

During a level load, samples short enough to be decoded up front queue their
decode here instead of decoding on the main thread. Workers run the same
stb_vorbis loop, upsampling and 16 bit conversion as the original path, with
their own copy of the compressed data and plain malloc; they never print.
Anything unusual (errors, the tolerated dropped-sample case, other rates)
fails the job, and the original path redoes it on the main thread.
idSoundCache::EndLevelLoad fills the OpenAL buffers on the main thread; a
sample played before then uses the existing software decoding path.

===============================================================================
*/

static idCVar s_threadedDecode( "s_threadedDecode", "1", CVAR_SOUND | CVAR_BOOL,
	"decode short OGG samples on worker threads during level loads" );
static idCVar s_verifyThreadedDecode( "s_verifyThreadedDecode", "0", CVAR_SOUND | CVAR_BOOL,
	"also decode threaded samples the original way and report any difference" );

namespace {
struct oggDecodeJob_t {
	idSoundSample *				sample = NULL;
	unsigned int				generation = 0;		// sample->dataGeneration when queued
	std::vector<unsigned char>	ogg;				// copy of the compressed file
	int							channels = 0;
	int							samplesPerSec = 0;
	int							objectSize = 0;
	int							length44k = 0;
	std::vector<short>			decoded;			// objectSize samples when ok
	bool						ok = false;
};

std::vector<std::unique_ptr<oggDecodeJob_t> >	oggJobs;
std::vector<std::thread>						oggWorkers;
std::mutex										oggLock;
std::condition_variable							oggWake;
size_t											oggNext = 0;
bool											oggFinish = false;

// idSampleDecoderLocal::DecodeOGG for a fresh decoder at offset zero, then the
// conversion of the load-time path. Returns false instead of printing.
bool DecodeOggJob( oggDecodeJob_t &job ) {
	const int channels = job.channels;
	const int shift = 22050 / job.samplesPerSec;
	std::vector<float> dest( job.length44k + 1 );

	int stbVorbErr = 0;
	stb_vorbis *stbv = stb_vorbis_open_memory( job.ogg.data(), (int)job.ogg.size(), &stbVorbErr, NULL );
	if ( !stbv ) {
		return false;
	}
	int totalSamples = job.length44k >> shift;
	int readSamples = 0;
	bool ok = true;
	do {
		float samplesBuf[2][MIXBUFFER_SAMPLES];
		float *samples[2] = { samplesBuf[0], samplesBuf[1] };
		const int reqSamples = Min( MIXBUFFER_SAMPLES, totalSamples / channels );
		if ( reqSamples == 0 ) {
			ok = false;		// the original's special case; leave it to that path
			break;
		}
		int ret = stb_vorbis_get_samples_float( stbv, channels, samples, reqSamples );
		if ( ret <= 0 ) {
			ok = false;		// errors and the tolerated-drop case
			break;
		}
		ret *= channels;
		SIMDProcessor->UpSampleOGGTo44kHz( dest.data() + ( readSamples << shift ), samples, ret, job.samplesPerSec, channels );
		readSamples += ret;
		totalSamples -= ret;
	} while ( totalSamples > 0 );
	stb_vorbis_close( stbv );
	if ( !ok ) {
		return false;
	}

	// idSampleDecoderLocal::Decode zeroes whatever was not decoded
	const int read44k = readSamples << shift;
	if ( read44k < job.length44k ) {
		memset( dest.data() + read44k, 0, ( job.length44k - read44k ) * sizeof( float ) );
	}
	R_OggFloatsToShorts( dest.data(), job.objectSize, job.samplesPerSec );
	job.decoded.assign( (const short *)dest.data(), (const short *)dest.data() + job.objectSize );
	return true;
}

void OggDecodeWorker() {
	for ( ;; ) {
		oggDecodeJob_t *job;
		{
			std::unique_lock<std::mutex> lock( oggLock );
			oggWake.wait( lock, [] { return oggNext < oggJobs.size() || oggFinish; } );
			if ( oggNext >= oggJobs.size() ) {
				return;		// finishing and nothing left
			}
			job = oggJobs[ oggNext++ ].get();
		}
		bool ok = false;
		try {
			ok = DecodeOggJob( *job );
		} catch ( ... ) {
			ok = false;
		}
		std::lock_guard<std::mutex> lock( oggLock );
		job->ok = ok;
	}
}

// Waits for all queued decodes. Main thread.
void FinishOggWorkers() {
	{
		std::lock_guard<std::mutex> lock( oggLock );
		oggFinish = true;
	}
	oggWake.notify_all();
	for ( std::thread &worker : oggWorkers ) {
		if ( worker.joinable() ) {
			worker.join();
		}
	}
	oggWorkers.clear();
	oggFinish = false;
}
}

bool SubmitThreadedOggDecode( idSoundSample *sample ) {
	if ( !s_threadedDecode.GetBool() || !soundSystemLocal.soundCache || !soundSystemLocal.soundCache->InsideLevelLoad() ||
		!Sys_IsMainThread() || !sample->nonCacheData || sample->objectMemSize <= 0 ||
		( sample->objectInfo.nChannels != 1 && sample->objectInfo.nChannels != 2 ) ||
		( sample->objectInfo.nSamplesPerSec != 11025 && sample->objectInfo.nSamplesPerSec != 22050 &&
		  sample->objectInfo.nSamplesPerSec != 44100 ) ) {
		return false;
	}
	std::unique_ptr<oggDecodeJob_t> job( new oggDecodeJob_t );
	job->sample = sample;
	job->generation = sample->dataGeneration;
	job->ogg.assign( sample->nonCacheData, sample->nonCacheData + sample->objectMemSize );
	job->channels = sample->objectInfo.nChannels;
	job->samplesPerSec = sample->objectInfo.nSamplesPerSec;
	job->objectSize = sample->objectSize;
	job->length44k = sample->LengthIn44kHzSamples();
	{
		std::lock_guard<std::mutex> lock( oggLock );
		oggJobs.push_back( std::move( job ) );
	}
	if ( oggWorkers.empty() ) {
		unsigned int count = std::thread::hardware_concurrency();
		count = count > 1 ? Min( count - 1, 8u ) : 1;
		for ( unsigned int i = 0; i < count; i++ ) {
			try {
				oggWorkers.emplace_back( OggDecodeWorker );
			} catch ( ... ) {
				break;
			}
		}
		if ( oggWorkers.empty() ) {
			// no threads available: undo and decode now
			std::lock_guard<std::mutex> lock( oggLock );
			oggJobs.pop_back();
			return false;
		}
	}
	oggWake.notify_one();
	return true;
}

// Main thread: fills the OpenAL buffers of all queued decodes, in queue order.
static void CompleteThreadedOggDecodes() {
	if ( oggJobs.empty() ) {
		return;
	}
	FinishOggWorkers();
	int threaded = 0, fallback = 0, discarded = 0, mismatched = 0;
	for ( auto &job : oggJobs ) {
		idSoundSample *sample = job->sample;
		idScopedCriticalSection sampleLock( CRITICAL_SECTION_ONE );
		if ( sample->purged || sample->dataGeneration != job->generation ) {
			discarded++;	// reloaded or purged since it was queued
			continue;
		}
		if ( job->ok && s_verifyThreadedDecode.GetBool() ) {
			idSampleDecoder *decoder = idSampleDecoder::Alloc();
			float *check = (float *)soundCacheAllocator.Alloc( ( sample->LengthIn44kHzSamples() + 1 ) * sizeof( float ) );
			decoder->Decode( sample, 0, sample->LengthIn44kHzSamples(), check );
			R_OggFloatsToShorts( check, sample->objectSize, sample->objectInfo.nSamplesPerSec );
			if ( memcmp( check, job->decoded.data(), sample->objectSize * sizeof( short ) ) ) {
				mismatched++;
				common->Printf( "THREADED_SOUND_VERIFY MISMATCH %s\n", sample->name.c_str() );
			}
			soundCacheAllocator.Free( (byte *)check );
			idSampleDecoder::Free( decoder );
		}
		if ( job->ok ) {
			sample->DecodeOggToHardware( job->decoded.data() );
			threaded++;
		} else {
			sample->DecodeOggToHardware( NULL );
			fallback++;
		}
	}
	oggJobs.clear();
	oggNext = 0;
	common->Printf( "%5i sounds decoded on worker threads, %i on the main thread, %i discarded\n", threaded, fallback, discarded );
	if ( s_verifyThreadedDecode.GetBool() ) {
		common->Printf( "THREADED_SOUND_VERIFY checked=%d mismatches=%d\n", threaded, mismatched );
	}
}

/*
===================
idSoundCache::idSoundCache()
===================
*/
idSoundCache::idSoundCache() {
	soundCacheAllocator.Init();
	soundCacheAllocator.SetLockMemory( true );
	listCache.AssureSize( 1024, NULL );
	listCache.SetGranularity( 256 );
	insideLevelLoad = false;
}

/*
===================
idSoundCache::~idSoundCache()
===================
*/
idSoundCache::~idSoundCache() {
	// no worker may outlive the samples; drop results of an unfinished load
	FinishOggWorkers();
	oggJobs.clear();
	oggNext = 0;
	listCache.DeleteContents( true );
	soundCacheAllocator.Shutdown();
}

/*
===================
idSoundCache::::GetObject

returns a single cached object pointer
===================
*/
const idSoundSample* idSoundCache::GetObject( const int index ) const {
	if (index<0 || index>=listCache.Num()) {
		return NULL;
	}
	return listCache[index];
}

/*
===================
idSoundCache::FindSound

Adds a sound object to the cache and returns a handle for it.
===================
*/
idSoundSample *idSoundCache::FindSound( const idStr& filename, bool loadOnDemandOnly ) {
	idStr fname;

	fname = filename;
	fname.BackSlashesToSlashes();
	fname.ToLower();

	declManager->MediaPrint( "%s\n", fname.c_str() );

	// check to see if object is already in cache
	for( int i = 0; i < listCache.Num(); i++ ) {
		idSoundSample *def = listCache[i];
		if ( def && def->name == fname ) {
			def->levelLoadReferenced = true;
			if ( def->purged && !loadOnDemandOnly ) {
				def->Load();
			}
			return def;
		}
	}

	// create a new entry
	idSoundSample *def = new idSoundSample;

	int shandle = listCache.FindNull();
	if ( shandle != -1 ) {
		listCache[shandle] = def;
	} else {
		shandle = listCache.Append( def );
	}

	def->name = fname;
	def->levelLoadReferenced = true;
	def->onDemand = loadOnDemandOnly;
	def->purged = true;

	if ( !loadOnDemandOnly ) {
		// this may make it a default sound if it can't be loaded
		def->Load();
	}

	return def;
}

/*
===================
idSoundCache::ReloadSounds

Completely nukes the current cache
===================
*/
void idSoundCache::ReloadSounds( bool force ) {
	int i;

	for( i = 0; i < listCache.Num(); i++ ) {
		idSoundSample *def = listCache[i];
		if ( def ) {
			def->Reload( force );
		}
	}
}

/*
====================
BeginLevelLoad

Mark all file based images as currently unused,
but don't free anything.  Calls to ImageFromFile() will
either mark the image as used, or create a new image without
loading the actual data.
====================
*/
void idSoundCache::BeginLevelLoad() {
	insideLevelLoad = true;

	for ( int i = 0 ; i < listCache.Num() ; i++ ) {
		idSoundSample *sample = listCache[ i ];
		if ( !sample ) {
			continue;
		}

		if ( com_purgeAll.GetBool() ) {
			sample->PurgeSoundSample();
		}

		sample->levelLoadReferenced = false;
	}

	soundCacheAllocator.FreeEmptyBaseBlocks();
}

/*
====================
EndLevelLoad

Free all samples marked as unused
====================
*/
void idSoundCache::EndLevelLoad() {
	int	useCount, purgeCount;
	common->Printf( "----- idSoundCache::EndLevelLoad -----\n" );

	// fill the buffers of samples decoded on worker threads during the load
	CompleteThreadedOggDecodes();

	insideLevelLoad = false;

	// purge the ones we don't need
	useCount = 0;
	purgeCount = 0;
	for ( int i = 0 ; i < listCache.Num() ; i++ ) {
		idSoundSample	*sample = listCache[ i ];
		if ( !sample ) {
			continue;
		}
		if ( sample->purged ) {
			continue;
		}
		if ( !sample->levelLoadReferenced ) {
//			common->Printf( "Purging %s\n", sample->name.c_str() );
			purgeCount += sample->objectMemSize;
			sample->PurgeSoundSample();
		} else {
			useCount += sample->objectMemSize;
		}
	}

	soundCacheAllocator.FreeEmptyBaseBlocks();

	common->Printf( "%5ik referenced\n", useCount / 1024 );
	common->Printf( "%5ik purged\n", purgeCount / 1024 );
}

/*
===================
idSoundCache::PrintMemInfo
===================
*/
void idSoundCache::PrintMemInfo( MemInfo_t *mi ) {
	int i, j, num = 0, total = 0;
	int *sortIndex;
	idFile *f;

	f = fileSystem->OpenFileWrite( mi->filebase + "_sounds.txt" );
	if ( !f ) {
		return;
	}

	// count
	for ( i = 0; i < listCache.Num(); i++, num++ ) {
		if ( !listCache[i] ) {
			break;
		}
	}

	// sort first
	sortIndex = new int[num];

	for ( i = 0; i < num; i++ ) {
		sortIndex[i] = i;
	}

	for ( i = 0; i < num - 1; i++ ) {
		for ( j = i + 1; j < num; j++ ) {
			if ( listCache[sortIndex[i]]->objectMemSize < listCache[sortIndex[j]]->objectMemSize ) {
				int temp = sortIndex[i];
				sortIndex[i] = sortIndex[j];
				sortIndex[j] = temp;
			}
		}
	}

	// print next
	for ( i = 0; i < num; i++ ) {
		idSoundSample *sample = listCache[sortIndex[i]];

		// this is strange
		if ( !sample ) {
			continue;
		}

		total += sample->objectMemSize;
		f->Printf( "%s %s\n", idStr::FormatNumber( sample->objectMemSize ).c_str(), sample->name.c_str() );
	}

	mi->soundAssetsTotal = total;

	f->Printf( "\nTotal sound bytes allocated: %s\n", idStr::FormatNumber( total ).c_str() );
	fileSystem->CloseFile( f );
	delete[] sortIndex;
}


/*
==========================================================================

idSoundSample

==========================================================================
*/

/*
===================
idSoundSample::idSoundSample
===================
*/
idSoundSample::idSoundSample() {
	dataGeneration = 0;
	memset( &objectInfo, 0, sizeof(waveformatex_t) );
	objectSize = 0;
	objectMemSize = 0;
	nonCacheData = NULL;
	amplitudeData = NULL;
	openalBuffer = 0;
	hardwareBuffer = false;
	defaultSound = false;
	onDemand = false;
	purged = false;
	levelLoadReferenced = false;
}

/*
===================
idSoundSample::~idSoundSample
===================
*/
idSoundSample::~idSoundSample() {
	PurgeSoundSample();
}

/*
===================
idSoundSample::LengthIn44kHzSamples
===================
*/
int idSoundSample::LengthIn44kHzSamples( void ) const {
	// objectSize is samples
	if ( objectInfo.nSamplesPerSec == 11025 ) {
		return objectSize << 2;
	} else if ( objectInfo.nSamplesPerSec == 22050 ) {
		return objectSize << 1;
	} else {
		return objectSize << 0;
	}
}

/*
===================
idSoundSample::MakeDefault
===================
*/
void idSoundSample::MakeDefault( void ) {
	idScopedCriticalSection sampleLock(CRITICAL_SECTION_ONE);
	++dataGeneration;
	int		i;
	float	v;
	int		sample;

	memset( &objectInfo, 0, sizeof( objectInfo ) );

	objectInfo.nChannels = 1;
	objectInfo.wBitsPerSample = 16;
	objectInfo.nSamplesPerSec = 44100;

	objectSize = MIXBUFFER_SAMPLES * 2;
	objectMemSize = objectSize * sizeof( short );

	nonCacheData = (byte *)soundCacheAllocator.Alloc( objectMemSize );

	short *ncd = (short *)nonCacheData;

	for ( i = 0; i < MIXBUFFER_SAMPLES; i ++ ) {
		v = sin( idMath::PI * 2 * i / 64 );
		sample = v * 0x4000;
		ncd[i*2+0] = sample;
		ncd[i*2+1] = sample;
	}

	alGetError();
	alGenBuffers( 1, &openalBuffer );
	if ( alGetError() != AL_NO_ERROR ) {
		common->Error( "idSoundCache: error generating OpenAL hardware buffer" );
	}

	alGetError();
	alBufferData( openalBuffer, objectInfo.nChannels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16, nonCacheData, objectMemSize, objectInfo.nSamplesPerSec );
	if ( alGetError() != AL_NO_ERROR ) {
		common->Warning( "idSoundCache: error loading data into OpenAL hardware buffer" );
		hardwareBuffer = false;
	} else {
		hardwareBuffer = true;
	}

	defaultSound = true;
}

/*
===================
idSoundSample::CheckForDownSample
===================
*/
void idSoundSample::CheckForDownSample( void ) {
	if ( !idSoundSystemLocal::s_force22kHz.GetBool() ) {
		return;
	}
	if ( objectInfo.wFormatTag != WAVE_FORMAT_TAG_PCM || objectInfo.nSamplesPerSec != 44100 ) {
		return;
	}
	int shortSamples = objectSize >> 1;
	short *converted = (short *)soundCacheAllocator.Alloc( shortSamples * sizeof( short ) );

	if ( objectInfo.nChannels == 1 ) {
		for ( int i = 0; i < shortSamples; i++ ) {
			converted[i] = ((short *)nonCacheData)[i*2];
		}
	} else {
		for ( int i = 0; i < shortSamples; i += 2 ) {
			converted[i+0] = ((short *)nonCacheData)[i*2+0];
			converted[i+1] = ((short *)nonCacheData)[i*2+1];
		}
	}
	soundCacheAllocator.Free( nonCacheData );
	nonCacheData = (byte *)converted;
	objectSize >>= 1;
	objectMemSize >>= 1;
	objectInfo.nAvgBytesPerSec >>= 1;
	objectInfo.nSamplesPerSec >>= 1;
}

/*
===================
idSoundSample::GetNewTimeStamp
===================
*/
ID_TIME_T idSoundSample::GetNewTimeStamp( void ) const {
	ID_TIME_T timestamp;

	fileSystem->ReadFile( name, NULL, &timestamp );
	if ( timestamp == FILE_NOT_FOUND_TIMESTAMP ) {
		idStr oggName = name;
		oggName.SetFileExtension( ".ogg" );
		fileSystem->ReadFile( oggName, NULL, &timestamp );
	}
	return timestamp;
}

/*
===================
idSoundSample::Load

Loads based on name, possibly doing a MakeDefault if necessary
===================
*/
void idSoundSample::Load( void ) {
	idScopedCriticalSection sampleLock(CRITICAL_SECTION_ONE);
	++dataGeneration;
	defaultSound = false;
	purged = false;
	hardwareBuffer = false;

	timestamp = GetNewTimeStamp();

	if ( timestamp == FILE_NOT_FOUND_TIMESTAMP ) {
		common->Warning( "Couldn't load sound '%s' using default", name.c_str() );
		MakeDefault();
		return;
	}

	// load it
	idWaveFile	fh;
	waveformatex_t info;

	if ( fh.Open( name, &info ) == -1 ) {
		common->Warning( "Couldn't load sound '%s' using default", name.c_str() );
		MakeDefault();
		return;
	}

	if ( info.nChannels != 1 && info.nChannels != 2 ) {
		common->Warning( "idSoundSample: %s has %i channels, using default", name.c_str(), info.nChannels );
		fh.Close();
		MakeDefault();
		return;
	}

	/*
	if ( info.wBitsPerSample != 16 ) {
		common->Warning( "idSoundSample: %s is %dbits, expected 16bits using default", name.c_str(), info.wBitsPerSample );
		fh.Close();
		MakeDefault();
		return;
	}
	*/

	/*
	if ( info.nSamplesPerSec != 44100 && info.nSamplesPerSec != 22050 && info.nSamplesPerSec != 11025 ) {
		common->Warning( "idSoundCache: %s is %dHz, expected 11025, 22050 or 44100 Hz. Using default", name.c_str(), info.nSamplesPerSec );
		fh.Close();
		MakeDefault();
		return;
	}
	*/

	objectInfo = info;
	objectSize = fh.GetOutputSize();
	objectMemSize = fh.GetMemorySize();

	nonCacheData = (byte *)soundCacheAllocator.Alloc( objectMemSize );
	fh.Read( nonCacheData, objectMemSize, NULL );

	// optionally convert it to 22kHz to save memory
	CheckForDownSample();

	// create hardware audio buffers
	// PCM loads directly
	if ( objectInfo.wFormatTag == WAVE_FORMAT_TAG_PCM ) {
		alGetError();
		alGenBuffers( 1, &openalBuffer );
		if ( alGetError() != AL_NO_ERROR )
			common->Error( "idSoundCache: error generating OpenAL hardware buffer" );
		if ( alIsBuffer( openalBuffer ) ) {
			alGetError();
			alBufferData( openalBuffer, objectInfo.nChannels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16, nonCacheData, objectMemSize, objectInfo.nSamplesPerSec );
			if ( alGetError() != AL_NO_ERROR ) {
				common->Warning( "idSoundCache: error loading data into OpenAL hardware buffer" );
				hardwareBuffer = false;
			} else {
				hardwareBuffer = true;
			}
		}
	}

	{
		// OGG decompressed at load time (when smaller than s_decompressionLimit seconds, 6 seconds by default)
		if ( objectInfo.wFormatTag == WAVE_FORMAT_TAG_OGG ) {
			if ( ( objectSize < ( ( int ) objectInfo.nSamplesPerSec * idSoundSystemLocal::s_decompressionLimit.GetInteger() ) ) ) {
				// During a level load the decode may run on a worker thread;
				// the buffer is then filled by idSoundCache::EndLevelLoad.
				if ( !SubmitThreadedOggDecode( this ) ) {
					DecodeOggToHardware( NULL );
				}
			}
		}
	}

	fh.Close();
}

/*
===================
idSoundSample::DecodeOggToHardware

The load-time OGG path: decodes the whole sample to 16 bit at its own rate
and fills an OpenAL buffer. preDecoded, when given, holds that exact data
(objectSize shorts) decoded on a worker; otherwise it is decoded here.
===================
*/
void idSoundSample::DecodeOggToHardware( const short *preDecoded ) {
	alGetError();
	alGenBuffers( 1, &openalBuffer );
	if ( alGetError() != AL_NO_ERROR )
		common->Error( "idSoundCache: error generating OpenAL hardware buffer" );
	if ( alIsBuffer( openalBuffer ) ) {
		if ( preDecoded ) {
			alGetError();
			alBufferData( openalBuffer, objectInfo.nChannels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16, preDecoded, objectSize * sizeof( short ), objectInfo.nSamplesPerSec );
			if ( alGetError() != AL_NO_ERROR ) {
				common->Warning( "idSoundCache: error loading data into OpenAL hardware buffer" );
				hardwareBuffer = false;
			} else {
				hardwareBuffer = true;
			}
			return;
		}

		idSampleDecoder *decoder = idSampleDecoder::Alloc();
		float *destData = (float *)soundCacheAllocator.Alloc( ( LengthIn44kHzSamples() + 1 ) * sizeof( float ) );

		// Decoder *always* outputs 44 kHz data
		decoder->Decode( this, 0, LengthIn44kHzSamples(), destData );

		// Downsample back to original frequency (save memory)
		R_OggFloatsToShorts( destData, objectSize, objectInfo.nSamplesPerSec );

		alGetError();
		alBufferData( openalBuffer, objectInfo.nChannels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16, destData, objectSize * sizeof( short ), objectInfo.nSamplesPerSec );
		if ( alGetError() != AL_NO_ERROR ) {
			common->Warning( "idSoundCache: error loading data into OpenAL hardware buffer" );
			hardwareBuffer = false;
		} else {
			hardwareBuffer = true;
		}

		soundCacheAllocator.Free( (byte *)destData );
		idSampleDecoder::Free( decoder );
	}
}

/*
===================
R_OggFloatsToShorts

Converts decoded 44 kHz floats in place to objectSize 16 bit samples at the
sample's own rate, as the load-time OGG path always has. Thread-safe.
===================
*/
void R_OggFloatsToShorts( float *destData, int objectSize, int samplesPerSec ) {
	if ( samplesPerSec == 11025 ) {
		for ( int i = 0; i < objectSize; i++ ) {
			if ( destData[i*4] < -32768.0f )
				((short *)destData)[i] = -32768;
			else if ( destData[i*4] > 32767.0f )
				((short *)destData)[i] = 32767;
			else
				((short *)destData)[i] = idMath::FtoiFast( destData[i*4] );
		}
	} else if ( samplesPerSec == 22050 ) {
		for ( int i = 0; i < objectSize; i++ ) {
			if ( destData[i*2] < -32768.0f )
				((short *)destData)[i] = -32768;
			else if ( destData[i*2] > 32767.0f )
				((short *)destData)[i] = 32767;
			else
				((short *)destData)[i] = idMath::FtoiFast( destData[i*2] );
		}
	} else {
		for ( int i = 0; i < objectSize; i++ ) {
			if ( destData[i] < -32768.0f )
				((short *)destData)[i] = -32768;
			else if ( destData[i] > 32767.0f )
				((short *)destData)[i] = 32767;
			else
				((short *)destData)[i] = idMath::FtoiFast( destData[i] );
		}
	}
}

/*
===================
idSoundSample::PurgeSoundSample
===================
*/
void idSoundSample::PurgeSoundSample() {
	// Vorbis decoders borrow nonCacheData; exclude in-flight decoding before
	// freeing it and invalidate surviving decoder instances even on address reuse.
	idScopedCriticalSection sampleLock(CRITICAL_SECTION_ONE);
	++dataGeneration;
	purged = true;

	alGetError();
	alDeleteBuffers( 1, &openalBuffer );
	if ( alGetError() != AL_NO_ERROR ) {
		common->Warning( "idSoundCache: error unloading data from OpenAL hardware buffer" );
	}

	openalBuffer = 0;
	hardwareBuffer = false;

	if ( amplitudeData ) {
		soundCacheAllocator.Free( amplitudeData );
		amplitudeData = NULL;
	}

	if ( nonCacheData ) {
		soundCacheAllocator.Free( nonCacheData );
		nonCacheData = NULL;
	}
}

/*
===================
idSoundSample::Reload
===================
*/
void idSoundSample::Reload( bool force ) {
	if ( !force ) {
		ID_TIME_T newTimestamp;

		// check the timestamp
		newTimestamp = GetNewTimeStamp();

		if ( newTimestamp == FILE_NOT_FOUND_TIMESTAMP ) {
			if ( !defaultSound ) {
				common->Warning( "Couldn't load sound '%s' using default", name.c_str() );
				MakeDefault();
			}
			return;
		}
		if ( newTimestamp == timestamp ) {
			return;	// don't need to reload it
		}
	}

	common->Printf( "reloading %s\n", name.c_str() );
	PurgeSoundSample();
	Load();
}

/*
===================
idSoundSample::FetchFromCache

Returns true on success.
===================
*/
bool idSoundSample::FetchFromCache( int offset, const byte **output, int *position, int *size, const bool allowIO ) {
	offset &= 0xfffffffe;

	if ( objectSize == 0 || offset < 0 || offset > objectSize * (int)sizeof( short ) || !nonCacheData ) {
		return false;
	}

	if ( output ) {
		*output = nonCacheData + offset;
	}
	if ( position ) {
		*position = 0;
	}
	if ( size ) {
		*size = objectSize * sizeof( short ) - offset;
		if ( *size > SCACHE_SIZE ) {
			*size = SCACHE_SIZE;
		}
	}
	return true;
}
