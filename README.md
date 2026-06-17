# ConAppsCM
ConAppsCM is a linux CMake project that provides infrastructure for developing linux console applications
with a CLI menu system. The project is open-source and available on GitHub.

## Initial Repo setup
```bash
git init
git add README.md
git commit -m "first commit"
git remote add origin https://github.com/bsburns/ConAppsCM.git
git push -u origin main
```

### Notes
If you haven't configured git on the system:
```bash
git config --global user.email "barry.s.burns@gmail.com"
git config --global user.name "Barry S. Burns"
```

## Pulling Repo
```bash
git clone https://github.com/bsburns/ConAppsCM.git
```

## Pushing Changes
```bash
git add <files>
git commit -a -m 'message'
git push -u origin main
```

## Update Repo:
```bash
git pull -a
```

## Linux CMake build:
### Intial build:
```bash
cmake -B build -G Ninja
ninja -C build 
```

### Subsequent builds:
```bash
ninja -C build 
```

## Corvette
### Pull 
- On laptop: open Git bash
```bash
cd /h/GitRepos
git clone https://github.com/bsburns/ConAppsCM.git
```
- Open VNC: corvette-1.ast.lmco.com:2 
- Copy repo to corvette-linux-dev
```bash
rsync -azv <repoName> barry@192.168.1.101:~/GitRepos/<repoName>
```
- Open terminal on corvette-lnux-dev and navigate to repo
```bash
ssh 192.168.1.101 -l barry
cd GitRepos/<repoName>
```
### Push
- Open terminal on corvette-linux-dev and navigate to repo
```bash
ssh 192.168.1.101 -l barry
cd GitRepos/
rsync -azv <repoName> e453331@192.168.1.11:~/GitRepos/<repoName>

```
# Install Boost
## Linux
```bash
sudo apt update
sudo apt install libboost-all-dev
```
## Windows
- Download boost from https://www.boost.org/users/download/
- Unzip to C:\local
```bash
cd C:\local\boost_1_91_0\
.\bootstrap.bat"
.\b2 --build-dir=build\x86 address-model=32 threading=multi --stagedir=.\bin\x86 --toolset=msvc -j 16 link=static,shared runtime-link=static,shared --variant=debug,release
.\b2 --build-dir=build\x64 address-model=64 threading=multi --stagedir=.\bin\x64 --toolset=msvc -j 8 link=static,shared runtime-link=static,shared --variant=debug,release
```
- Add to CMakeLists.txt
```cmake
if (WIN32)
    set(Boost_DIR "C:\\local\\boost_1_91_0\\bin\\x64\\lib\\cmake\\Boost-1.91.0")
endif()
find_package(Boost REQUIRED)
```
- There is some issue with looger.h and boost log. I had to move include of logger.h to files with boost include files to get it to compile:
