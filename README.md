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
