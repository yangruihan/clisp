# CLisp

Lisp written in pure C (C99)

## Run

```sh
# clone repository
git clone https://github.com/yangruihan/clisp.git

cd clisp

# update submodule
git submoudle init
git submodule update

# build
mkdir build
cd build
cmake ..
make -j6

# run
cd ..
./bin/cmal/Debug/cmal
```

## REPL

```sh
./bin/cmal/Debug/cmal

user> (println "Hello World")
Hello World
nil
user>
```

## Run With File

```sh
./bin/cmal/Debug/cmal mal/stepA_mal.mal

Mal [c-mal]
nil
mal-user>
```

## tutorial

[The Make-A-Lisp Process](https://github.com/kanaka/mal/blob/master/process/guide.md)