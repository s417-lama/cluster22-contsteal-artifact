# Isola

Isola is a tool to manage multiple versions of a project in isolated directories (called *isola*s) with similar APIs to containers (e.g., Docker).
Isola is designed for performance evaluation of runtimes and libraries with multiple build settings in clusters or supercomputers.

Isola provides a virtualized and isolated HOME directory (given by `$ISOLA_HOME`) instead of virtualizing the entire root directory by `chroot` as containers do.
Although the feature of Isola is limited compared to containers, Isola does not need any privilege.

To summarize the benefits of Isola, it
- Simplifies the management of different build settings and multiple versions of a program (just like creating multiple Docker images)
- Enables parallel execution of programs with different build settings by isolating directories for each execution (just like running multiple Docker containers in parallel)
- Provides a simple way to create a disposable copy of the current directory (just like running Docker containers with `--rm` option)

## Install

```sh
./install.bash
```

will install Isola at `~/.isola`.

Path setting (e.g., in `.bashrc`):
```sh
export PATH=~/.isola/bin:$PATH
```

To change the install path, run:

```sh
ISOLA_ROOT=/path/to/install ./install.bash
```

To uninstall, just remove the `.isola` dir (e.g., `rm -rf ~/.isola`).

## Tutorial

- [A Tutorial on Managing Multiple Builds of OpenBLAS](/tutorial/openblas/README.md)

## Features

### Managing Multiple Versions of a Project in Isolated Directories

Multiple versions of a project can be efficiently managed by Git, but it is sometimes insufficient for running multiple versions of a program simultaneously, especially for clusters or supercomputers where a file system is shared.
In addition, managing build artifacts (such as executable binary and libraries) in Git is not recommended.

Thus, creating isolated directories is beneficial for running multiple experiments in parallel and managing different build settings separately.
Isola provides a simple way for managing multiple versions of a project in isolated directories in a similar way to containers (e.g., Docker).

In the following, I will give a simple example of directory isolation with two projects where one project relies on the other.

```sh
## Create two projects
## - project1 : will contain a shell script (test.sh)
## - project2 : will contain a shell script (run.sh) that runs test.sh in project1
$ mkdir project1 project2

$ cd project1

## Create a shell script 'test.sh'
$ echo "echo foo" > test.sh

## Create an isola named 'foo'.
## 'ISOLA_HOME' environment variable is set during the creation,
## so it checks by echoing $ISOLA_HOME
$ isola create foo bash -c 'echo isola HOME is at $ISOLA_HOME'
isola HOME is at ~/.isola/projects/project1/foo/2020-07-02_18-41-13

Isola 'project1:foo:2020-07-02_18-41-13' was successfully created.

## The format of an isola ID is '<project>:<name>:<version>'.
## - <project> : the directory name where `isola create` is executed
## - <name>    : the name specified in `isola create` command
## - <version> : timestamp is automatically given. 'latest' is an alias to the latest version

## Create another version of 'test.sh'
$ echo "echo bar" > test.sh

## Create another isola named 'bar'
$ isola create bar bash -c 'echo isola HOME is at $ISOLA_HOME'
isola HOME is at ~/.isola/projects/project1/bar/2020-07-02_18-41-26

Isola 'project1:bar:2020-07-02_18-41-26' was successfully created.

## List all isolas
$ isola ls -r
project1:bar:2020-07-02_18-41-26
project1:bar:latest
project1:foo:2020-07-02_18-41-13
project1:foo:latest

$ cd ../project2/

## Create a shell script that runs 'test.sh' in project1 and
## stores the result to 'output.txt'
$ echo 'sh $ISOLA_HOME/project1/test.sh > output.txt' > run.sh
$ ls
run.sh

## Create an isola based on the latest version of 'project1:foo' by running 'run.sh'
$ isola create -b project1:foo foo sh run.sh

Isola 'project2:foo:2020-07-02_18-42-56' was successfully created.

## Create an isola based on the latest version of 'project1:bar' by running 'run.sh'
$ isola create -b project1:bar bar sh run.sh

Isola 'project2:bar:2020-07-02_18-43-01' was successfully created.

## Check if 'project2:foo' and 'project2:bar' are created
$ isola ls -r
project1:bar:2020-07-02_18-41-26
project1:bar:latest
project1:foo:2020-07-02_18-41-13
project1:foo:latest
project2:bar:2020-07-02_18-43-01
project2:bar:latest
project2:foo:2020-07-02_18-42-56
project2:foo:latest

## Check 'output.txt' of each version by utilizing `isola where` command
$ cat $(isola where project2:foo)/project2/output.txt
foo
$ cat $(isola where project2:bar)/project2/output.txt
bar
```

This example demonstrated that, without modifying files in project2, 'run.sh' in project2 could refer to different versions of 'test.sh' in project1 (i.e., an application can run on a runtime with different build settings).
The location of 'output.txt' is also isolated, enabling parallel execution of different isolas.

The resulting directory structure looks like:
```
$ tree $(isola where project1:foo)
~/.isola/projects/project1/foo/2020-07-02_18-41-13
└── project1
    └── test.sh      ## echo foo

$ tree $(isola where project2:foo)
~/.isola/projects/project2/foo/2020-07-02_18-42-56
├── project1
│   └── test.sh      ## echo foo
└── project2
    ├── output.txt   ## foo
    └── run.sh       ## sh $ISOLA_HOME/project1/test.sh > output.txt

$ tree $(isola where project1:bar)
~/.isola/projects/project1/bar/2020-07-02_18-41-26
└── project1
    └── test.sh      ## echo bar

$ tree $(isola where project2:bar)
~/.isola/projects/project2/bar/2020-07-02_18-43-01
├── project1
│   └── test.sh      ## echo bar
└── project2
    ├── output.txt   ## bar
    └── run.sh       ## sh $ISOLA_HOME/project1/test.sh > output.txt
```

### Creating a Disposable Directory

If you want to create a disposable directory with the copy of the current directory, `isola run` command will be an useful command.
You can test some destructive operations to the current directory without affecting the original files.

The following example shows how to create an isolated disposable directory.

```sh
## Create 'isola_test' directory with file 'foo' and 'bar'
$ mkdir isola_test
$ cd isola_test
$ touch foo bar
$ ls
bar  foo

## Create a disposable directory with the copy of the current directory
## by 'isola run' command and launch bash on it
$ isola run bash

## 'isola_test' directory is copied to a tmp dir
$ pwd
~/.isola/tmp/c0758e9a-c052-4574-bdd4-5f7e063a9fc6/isola_test
$ ls
bar  foo

## Remove the 'foo' file
$ rm foo
$ ls
bar

## Exit and remove the current isola instance
$ exit
exit

## The file 'foo' exists in the original directory
$ pwd
~/isola_test
$ ls
bar  foo
```

## Commands

### `isola create`

Creates a new isola with `NAME` by running `COMMANDS`.
The curernt directory is automatically copied to under the `$ISOLA_HOME/` dir.

```
Usage: isola create [OPTIONS] NAME COMMANDS

OPTIONS:
  -b base_isola   speficy a base isola ID as 'project:name[:version]' format
  -o              overwrite the 'latest' isola with the new one (the current 'latest' isola will be removed)
  -r              make the isola readonly after creation
  -h              show help
```

The created isola will have an ID with the format `<project>:<name>:<version>`.
- `<project>` : the directory name where `isola create` is executed
- `<name>`    : the name specified in `isola create` command
- `<version>` : timestamp is automatically given. 'latest' is an alias to the latest version

On executing `COMMANDS`, `$ISOLA_HOME` environment variable is set.

### `isola run`

Creates a disposable isola where `COMMANDS` will run.
The curernt directory is automatically copied to under the `$ISOLA_HOME/` dir.
The result is automatically removed unlike `isola create`.

```
Usage: isola run [OPTIONS] COMMANDS

OPTIONS:
  -b base_isola   speficy a base isola ID as 'project:name[:version]' format
  -n              do not copy the current project to the isola
  -h              show help
```

### `isola ls`

Lists saved isolas.
`isola ls -r` will show all versions of isolas.

```
Usage: isola ls [OPTIONS] [project[:name[:version]]]

OPTIONS:
  -r    list isolas recursively
  -h    show help
```

### `isola where`

Gets a path to the specified isola.

```
Usage: isola where [OPTIONS] project:name[:version]

OPTIONS:
  -h    show help
```

### `isola rm`

Removes the specified isolas.

```
Usage: isola rm [OPTIONS] project[:name[:version]]...

OPTIONS:
  -f    force removal without asking in prompt
  -h    show help
```

### `isola cleanup`

Tries to fix inconsistent state (e.g., the `latest` tag does not point to the latest isola).

```
Usage: isola cleanup [OPTIONS]

OPTIONS:
  -h    show help
```

## Known Problems

- When an isola (isola1) depends on another isola (isola2), removing isola2 is not recommended because some executables in isola1 may be linked to some libraries in isola1 by using absolute paths. This is a fundamental problem of Isola as it does not use `chroot` command to virtualize the HOME directory. This is not a problem as long as we do not remove isolas depended on by others.
