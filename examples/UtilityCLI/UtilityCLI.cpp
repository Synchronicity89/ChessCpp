// UtilityCLI.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cerrno>
#include <windows.h>

// Build issues provided (kept separate)
static const std::vector<std::string> kBuildIssues = {
    "C4505: ParseBoard: unreferenced function removed (main.cpp:111)",
    "C4505: ApplyUCIMoveToBoardSquares: unreferenced function removed (main.cpp:130)",
    "C4505: BuildFEN: unreferenced function removed (main.cpp:163)",
    "LNK2019: unresolved external symbol engine::generate_pseudo_moves referenced in engine::ab_search",
    "LNK2019: unresolved external symbol engine::filter_legal referenced in engine::ab_search",
    "LNK2019: unresolved external symbol engine::apply_move referenced in engine::ab_search",
    "LNK1120: 3 unresolved externals (example_sdl3_sdlgpu3.exe)"
};

struct FileScanResult { std::string path; bool ok; std::string note; };

static void printIssues(){ std::cout << "Known captured build issues (" << kBuildIssues.size() << "):\n"; for(const auto &s: kBuildIssues) std::cout << "  - " << s << '\n'; }

// Run a command and capture stdout (best-effort)
static std::string runCommand(const std::string& cmd){ std::string out; FILE* pipe = _popen(cmd.c_str(), "r"); if(!pipe){ return std::string("<failed to start>"); } char buffer[512]; while(fgets(buffer,sizeof(buffer),pipe)){ out += buffer; } _pclose(pipe); return out; }

// Recursive directory enumeration using Win32 API (no modifications performed)
static void enumerate(const std::string& root, std::vector<FileScanResult>& out, size_t maxFiles=2000){
    std::vector<std::string> stack; stack.push_back(root);
    size_t processed=0;
    while(!stack.empty() && processed < maxFiles){
        std::string cur = stack.back(); stack.pop_back();
        std::string pattern = cur + "\\*";
        WIN32_FIND_DATAA fdata; HANDLE h = FindFirstFileA(pattern.c_str(), &fdata);
        if(h == INVALID_HANDLE_VALUE){ out.push_back({cur,false,"FindFirstFile failed"}); continue; }
        do {
            const char* name = fdata.cFileName;
            if(strcmp(name,".")==0 || strcmp(name,"..")==0) continue;
            std::string path = cur + "\\" + name;
            if(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){ stack.push_back(path); }
            else {
                // Try open (read only)
                std::ifstream in(path, std::ios::binary);
                if(!in){ out.push_back({path,false,"open failed"}); }
                else {
                    // Read limited bytes to avoid huge memory usage
                    std::string firstBytes; firstBytes.resize(256);
                    in.read(&firstBytes[0], 256); size_t got = (size_t)in.gcount(); firstBytes.resize(got);
                    out.push_back({path,true, got?"ok":"empty"});
                }
                processed++;
                if(processed >= maxFiles) break;
            }
        } while(FindNextFileA(h,&fdata));
        FindClose(h);
    }
}

// Simple symbol presence check in source file
static bool sourceContains(const std::string& filePath, const std::string& token){ std::ifstream in(filePath); if(!in) return false; std::string line; while(std::getline(in,line)){ if(line.find(token)!=std::string::npos) return true; } return false; }

struct SymbolStatus { std::string name; bool found; };

static void analyzeEngine(const std::string& enginePath){
    std::vector<SymbolStatus> syms = {
        {"generate_pseudo_moves", false},
        {"filter_legal", false},
        {"apply_move", false},
        {"ab_search", false},
    };
    for(auto &s: syms){ s.found = sourceContains(enginePath, s.name + "("); }
    std::cout << "Engine source symbol presence in " << enginePath << ":\n";
    for(auto &s: syms){ std::cout << "  " << s.name << ": " << (s.found?"found":"missing") << '\n'; }
    // Root cause heuristic
    bool allPresent=true; for(auto &s: syms){ if(!s.found){ allPresent=false; break; } }
    std::cout << "Inference: ";
    if(allPresent){
        std::cout << "Symbols implemented; unresolved externals likely due to stale library or ODR mismatch (build config / project reference).\n";
    } else {
        std::cout << "One or more symbols not present in engine.cpp – source edit removed definitions or file excluded from build.\n";
    }
}

static void usage(){ std::cout << "UtilityCLI diagnostic usage:\n  UtilityCLI.exe --diagnose [rootDir] [--msbuild <proj>] [--depth <n>]\n"; }

int main(int argc, char** argv)
{
    bool diagnose=false; std::string rootDir="."; std::string msbuildProj; size_t depthLimit=1500;
    for(int i=1;i<argc;++i){ std::string a=argv[i]; if(a=="--diagnose"||a=="-d") diagnose=true; else if(a=="--msbuild" && i+1<argc){ msbuildProj=argv[++i]; } else if(a=="--depth" && i+1<argc){ depthLimit = (size_t)std::atoi(argv[++i]); } else if(a=="--root" && i+1<argc){ rootDir = argv[++i]; } else if(a=="--help"||a=="-h") { usage(); return 0; } }
    if(!diagnose){ usage(); return 0; }

    std::cout << "Diagnosing build/link issues...\n";
    printIssues();

    // Enumerate files
    std::cout << "\nScanning files under: " << rootDir << " (limit=" << depthLimit << ")\n";
    std::vector<FileScanResult> scans; enumerate(rootDir, scans, depthLimit);
    size_t failed=0; for(auto &r: scans) if(!r.ok) failed++;
    std::cout << "Scanned " << scans.size() << " items; failures=" << failed << '\n';
    for(auto &r: scans){ if(!r.ok) std::cout << "  [FAIL] " << r.path << " : " << r.note << '\n'; }

    // Engine source heuristic path (common location)
    std::string engineSrc = "chessnative2/engine.cpp"; // relative assumption
    analyzeEngine(engineSrc);

    if(!msbuildProj.empty()){
        std::cout << "\nInvoking MSBuild on: " << msbuildProj << '\n';
        std::string cmd = "msbuild \"" + msbuildProj + "\" /p:Configuration=Debug /p:Platform=x64";
        std::string output = runCommand(cmd);
        std::cout << output << '\n';
    }

    // Root cause summary
    bool unresolved = std::any_of(kBuildIssues.begin(), kBuildIssues.end(), [](const std::string& s){ return s.find("LNK2019")!=std::string::npos; });
    if(unresolved){
        std::cout << "\nSummary: Unresolved externals detected. Likely causes: \n"
                  << "  - Stale or mismatched library (run full clean + rebuild).\n"
                  << "  - Function signatures changed (name mangling mismatch).\n"
                  << "  - engine.cpp excluded from the build for test configuration.\n";
    }
    std::cout << "\nDiagnosis complete.\n";
    return unresolved? 2 : 0;
}

