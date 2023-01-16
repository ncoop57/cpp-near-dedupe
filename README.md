# cpp-near-dedupe
dedupes arrow datasets ( IPC .arrow db's at the moment) using minhash / jaccard similarity scores. Uses multiple threads to speed things up

# warning
this project was thrown together in a few days, so more improvements will come

# todo
- better readme
- more commandline arguments for more options
- better error checking
- handle various arrow formats
- more perf and ram optimizations
- thread affinities / priority tweaking
- handle different CPU intrinsics for more hardware support
- unit tests
- file write permissions on output folder
- clearing output folder on run
- allowing to operate inplace on a dataset
- allow continuing from a partially crunched set of data

# building
- windows

```
visual studio:
open the sln (for sln based ) or the folder with the cmakelists.txt file (for cmake based)
```

- WSL ( tested on ubuntu )
due to accessing windows drives, i had to sudo every command to avoid errors:
```
Cmake:
sudo cmake .
sudo make release
```

- linux ( tested on ubuntu )

```
Cmake:
cmake .
make release
```

# running
- windows/linux

```
executing the program with no cmd args will display the expected cmd args
```

# bugs and issues
- need to manually clear the output folder / make sure its empty / ensure you have permissions, otherwise std::filesystem:: throws an exception and it fails at the end of crunching.
