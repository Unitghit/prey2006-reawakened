"""Compile the production ASE color-face parser against a poisoned-memory fixture.
Requires clang++ on PATH. No retail assets or engine runtime required.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "neo/renderer/Model_ase.cpp").read_text()
start = source.index("static void ASE_KeyCFACE_LIST(")
end = source.index("static void ASE_KeyMESH_TVERTLIST(", start)
function = source[start:end]
harness = r'''#include <cstdlib>
#include <cstring>
#include <cstdio>
struct Face { unsigned char vertexColors[3][4]; };
struct aseMesh_t { Face *faces; float (*cvertexes)[3]; };
struct { int currentFace; char token[32]; } ase;
Face face;
float colors[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
aseMesh_t mesh = { &face, colors };
aseMesh_t *ASE_GetCurrentMesh() { return &mesh; }
const char *tokens[] = {"0", "0", "1", "2"};
int tokenIndex;
void ASE_GetToken(bool) { std::snprintf(ase.token, sizeof(ase.token), "%s", tokens[tokenIndex++]); }
struct Common { void Error(const char *, const char *) { std::abort(); } } c;
Common *common = &c;
''' + function + r'''
int main() {
    const unsigned char expected[3][4] = {{255,0,0,255},{0,0,255,255},{0,255,0,255}};
    for (int poison = 0; poison < 256; ++poison) {
        std::memset(&face, poison, sizeof(face));
        ase.currentFace = 0;
        tokenIndex = 0;
        ASE_KeyCFACE_LIST("*MESH_CFACE");
        if (std::memcmp(face.vertexColors, expected, sizeof(expected)) || ase.currentFace != 1)
            return 1;
    }
    std::puts("PASS: RGB, winding and opaque alpha independent of 256 allocation patterns");
}
'''
with tempfile.TemporaryDirectory(prefix="prey-ase-alpha-") as directory:
    directory = Path(directory)
    cpp = directory / "test.cpp"
    exe = directory / "test.exe"
    cpp.write_text(harness)
    subprocess.run(["clang++", str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
