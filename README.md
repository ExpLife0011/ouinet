# Ouinet

**Note:** The steps described below have only been tested to work on GNU/Linux
on AMD64 platforms.

## Using the easy installation script

The [scripts/build-ouinet.sh][build-ouinet.sh] script can be used to download
all necessary source code, build the Ouinet library and tools and install
them, all with a simple command invocation.  If you do not already have it in
your computer, just download it and copy it to some temporary directory.  Then
open a shell in that directory and run:

    sh build-ouinet.sh

[build-ouinet.sh]: https://raw.githubusercontent.com/equalitie/ouinet/master/scripts/build-ouinet.sh

The script will first check that you have all the needed system packages.  If
you do not, it will show an error like:

    Missing dependencies:  some-package some-other-package
    Ignore this warning with --force.

The names of missing dependencies correspond to package names in a Debian or
Ubuntu-based system.  To install them, just run:

    sudo apt update
    sudo apt install some-package some-other-package

In other platforms the names of packages may differ and you may need to figure
them out and install them manually.

After installing missing packages you can run the script again:

    sh build-ouinet.sh

You may need to repeat this until the script succeeds and reports instructions
on how to run the client or injector tools.  The whole process takes a few
minutes and requires around 2 GB of storage.

## Requirements

* `cmake` 3.5+
* `g++` capable of C++14
* The [Boost library](http://www.boost.org/)

Note: The Go language and the IPFS project will be downloaded automatically
during the build process.

## Clone

Ouinet uses Git submodules, thus to properly clone it, use

```
$ git clone --recursive git@github.com:equalitie/ouinet.git
```

OR

```
$ git clone git@github.com:equalitie/ouinet.git
$ cd ouinet
$ git submodule update --init --recursive
```

## Build

```
# Assuming:
#   * <PROJECT ROOT> points to the directory where the
#                    CMakeLists.txt file is
#   * <BUILD DIR> is a directory of your choice where all
#                 (even temporary) build files will go
$ mkdir -p <BUILD DIR>
$ cd <BUILD DIR>
$ cmake <PROJECT ROOT>
$ make
```

## Test

Before everything else, we need to start GNUnet services. To do that
open a new terminal window and execute:

```
$ ./scripts/start-gnunet-services.sh
```

Leave that script running and start another terminal window where we'll start
the injector:

```
$ ./injector --repo ../repos/injector
Swarm listening on /ip4/127.0.0.1/tcp/4001
Swarm listening on /ip4/192.168.0.136/tcp/4001
Swarm listening on /ip6/::1/tcp/4001
IPNS DB: <DB_IPNS>
...
GNUnet ID: <GNUNET_ID>
...
```

Make note of the `<DB_IPNS>` and `<GNUNET_ID>` strings in the above output,
we'll need to pass them as arguments to the client.

While injector is still running, start the client in yet another terminal
window and pass it the injector's `<GNUNET_ID>` and `<DB_IPNS>` strings from
above:

```
$ ./client --repo ../repos/client \
           --injector-ipns <DB_IPNS> \
           --injector-ep <GNUNET_ID>:injector-main-port
```

Now [modify the settings of your
browser](http://www.wikihow.com/Enter-Proxy-Settings-in-Firefox) to make the
client - which runs on port localhost:7070 - it's proxy. Also **make sure
'localhost' is not listed in the `"No Proxy for"` field**. Once done, you can
enter `localhost` into your browser and it should show you what database of
sites the client is currently using.

It is likely that at first the database shall be `nill` which indicates that
no database has been dowloaded from IPFS yet. This may take from a couple of
seconds up to about three minutes. The page refreshes itself regurarly so
once the client downloads the database, it should display automatically.

In the mean time, notice also the small form at the top of the page looking
something like this:

```
Injector proxy: enabled [disable]
```

This means that proxing to injector is currently `enabled`, which in turn
means that if one points the browser to a non secure http page and the page
isn't yet in the IPFS database, then the client shall forward the HTTP
request to the injector. On success, injector will (A) send the content
back and (B) upload the content to the IPFS database.

Each time the injector updates the database it prints out a message:

```
Published DB: Qm...
```

Once published, it will take some time for the client to download it
(up to three minutes from experience) and once it does so, it will be shown
on client's frontend.

At that point one can disable the proxying through injector, clear
browser's cached data and try to point the browser to the same non secured
HTTP page.

```
$ ./test.sh <BUILD DIR>/client
```

