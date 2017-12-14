# Ouinet

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

