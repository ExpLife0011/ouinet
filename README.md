[![CircleCI](https://circleci.com/gh/equalitie/ouinet/tree/master.svg?style=shield)](https://circleci.com/gh/equalitie/ouinet/tree/master)

# Ouinet

**Ouinet** is a Free/Open Source technology which allows web content to be
served with the help of an entire network of cooperating nodes using
peer-to-peer routing and distributed caching of responses.  This helps
mitigate the Web's characteristic single point of failure due to a client
application not being able to connect to a particular server.

The typical Ouinet *client* node setup consists of a web browser or other
application using a special HTTP proxy or API provided by a dedicated program
or library on the local machine.  When the client gets a request for content,
it attempts to retrieve the resource using several mechanisms.  It tries to
fetch the page from a distributed cache (like [IPFS][]), and if not available,
it contacts a trusted *injector* server over a peer-to-peer routing system
(like [I2P][]) and asks it to fetch the page and store it in the *distributed
cache*.

[IPFS]: https://ipfs.io/ "InterPlanetary File System"
[I2P]: https://geti2p.net/ "Invisible Internet Project"

Future accesses by client nodes to popular content inserted in distributed
storage shall benefit from an increased redundancy and locality, which
translates in increased availability in the face of connectivity problems,
increased transfer speeds in case or poor upstream links, and reduced
bandwidth costs when access providers charge more for external or
international traffic.  Content injection is also designed in a way which
allows for content re-introduction and seeding on extreme cases of total
connectivity loss (e.g. natural disasters).

The Ouinet library is a core technology that can be used by any application to
benefit from these advantages.  Ouinet integration provides any content
creator the opportunity to use cooperative networking and storage for the
delivery of their content to users around the world.

**Warning:** Ouinet is still highly experimental.  Some features (like
peer-to-peer routing) may or may not not work smoothly depending on the
different back end technologies.  Information about your browsing might be
leaked to other participants in the network.  Running some components (like
injector code) may turn your computer into an open web proxy, and other
security or privacy-affecting issues might exist.  Please keep this in mind
when using this software.

**Note:** The steps described below have only been tested to work on GNU/Linux
on AMD64 platforms.

## Cloning the source tree

Ouinet uses Git submodules, thus to properly clone it, use:

    $ git clone --recursive https://github.com/equalitie/ouinet.git

You can also clone and update the modules separately:

    $ git clone https://github.com/equalitie/ouinet.git
    $ cd ouinet
    $ git submodule update --init --recursive

## Build requirements (desktop)

To build Ouinet natively on your system, you will need the following software
to be already available:

* CMake 3.5+
* `g++` capable of C++14
* The [Boost library](http://www.boost.org/) 1.67+

Assuming that `<PROJECT ROOT>` points to the directory where the
`CMakeLists.txt` file is, and `<BUILD DIR>` is a directory of your choice
where all (even temporary) build files will go, you can build Ouinet with:

    $ mkdir -p <BUILD DIR>
    $ cd <BUILD DIR>
    $ cmake <PROJECT ROOT>
    $ make

However, we encourage you to use a Vagrant environment for development, or
Docker containers for deploying a Ouinet client or an injector.  These have a
different set of requirements.  See the corresponding sections below for
further instructions on Vagrant and Docker.

## Using a Vagrant environment

One of the easiest ways to build Ouinet from source code (e.g. for development
or testing changes and fixes to code) is using a [Vagrant][] development
environment.

[Vagrant]: https://www.vagrantup.com/

To install Vagrant on a Debian system, run:

    $ sudo apt-get install vagrant

Ouinet's source tree contains a `Vagrantfile` which allows you to start a
Vagrant environment ready to build and run Ouinet by entering the source
directory and executing:

    $ vagrant up

If your Vagrant installation uses VirtualBox by default and you find problems,
you may need to force it to use libvirt instead:

    $ sudo apt-get install libvirt-bin libvirt-dev
    $ vagrant plugin install vagrant-libvirt
    $ vagrant up --provider=libvirt

### Building Ouinet in Vagrant

Enter the Vagrant environment with `vagrant ssh`.  There you will find:

  - Your local Ouinet source tree mounted read-only under `/vagrant`.

  - Your local Ouinet source tree mounted read-write under `/vagrant-rw`.

  - `~vagrant/build-ouinet-git.sh`: Running this script will clone the Ouinet
    Git repository and all submodules into `$PWD/ouinet-git-source`  and build
    Ouinet into `$PWD/ouinet-git-build`.  Changes to source outside of the
    Vagrant environment will not affect this build.

  - `~vagrant/build-ouinet-local.sh`: Running this script will use your local
    Ouinet source tree (mounted under `/vagrant`) to build Ouinet into
    `$PWD/ouinet-local-build`.  Thus you can edit source files on your
    computer and have them built in a consistent environment.

    Please note that this requires that you keep submodules in your checkout
    up to date as indicated above.

### Accessing Ouinet services from your computer

The Vagrant environment is by default isolated, but you can configure it to
redirect ports from the host to the environment.

For instance, if you want to run a Ouinet client (with its default
configuration) in Vagrant and use it as a proxy in a browser on your computer,
you may uncomment the following line in `Vagrantfile`:

    #vm.vm.network "forwarded_port", guest: 8080, host: 8081, guest_ip: "127.0.0.1"

And restart the environment:

    $ vagrant halt
    $ vagrant up

Then you can configure your browser to use `localhost` port 8081 to contact
the HTTP proxy.

### Vagrant instance on AWS

The source tree also contains `Vagrantfile.aws`, which you can use to deploy
the Vagrant environment to Amazon Web Services (AWS):

    $ vagrant plugin install vagrant-aws
    $ vagrant plugin install vagrant-sshfs

    $ export AWS_ACCESS_KEY_ID='YOUR_ACCESS_ID'
    $ export AWS_SECRET_ACCESS_KEY='your secret token'

    $ VAGRANT_VAGRANTFILE=Vagrantfile.aws vagrant up
    $ vagrant sshfs --mount linux
    $ vagrant ssh

## Testing

To perform some tests using the just-built Ouinet client and an existing
injector, you first need to know the *injector endpoint* and a *distributed
cache name*.  These use to be an IP address and PORT, and an IPNS identifier,
respectively (although the injector endpoint may also be an I2P peer
identity).

You need to configure the Ouinet client to use the aforementioned parameters.
Edit the included configuration file:

    edit ouinet-repos/client/ouinet-client.conf

Add options there for the injector endpoint and the distributed cache name.
Remember to replace the values with your own:

    injector-ep = 192.0.2.1:1234
    injector-ipns = Qm0123456789abcdefghijklmnopqrstuvwxyzABCDEFGI

All the steps above only need to be done once.

Now start the Ouinet client by running:

    ouinet-build/client --repo ouinet-repos/client

The client opens a web proxy on local port 8080 (see option `listen-on-tcp` in
its configuration file).  If you have Firefox installed, you can create a new
profile (stored under the `ff-profile` directory in the example below) which
uses the Ouinet client as a proxy by executing the following commands on
another shell:

    mkdir -p ff-profile
    env http_proxy='http://localhost:8080/' firefox --no-remote --profile ff-profile

When you access the web using this browser, your requests will go through your
local Ouinet client, which will attempt several mechanisms supported by Ouinet
to retrieve the resource.

When you are done testing the Ouinet client, just shut down the browser, the
client itself (by hitting Ctrl+C).

## Test

    First, we start the injector locally:
    
    $ ./injector --repo ../repos/injector
    Swarm listening on /ip4/127.0.0.1/tcp/4001
    Swarm listening on /ip4/192.168.0.136/tcp/4001
    Swarm listening on /ip6/::1/tcp/4001
    IPNS DB: <DB_IPNS>
    ...
    TCP Address: <IP:PORT>
    ...

Make note of the `<DB_IPNS>` and `<IP:PORT>` strings in the above output,
we'll need to pass them as arguments to the client.  You may also find these
values in the `cache-ipns` and `endpoint-tcp` files in the injector's
repository root directory (`../repos/injector` in the example).

While injector is still running, start the client in yet another terminal
window and pass it the injector's `<IP:PORT>` and `<DB_IPNS>` strings from
above:

    $ ./client --repo ../repos/client \
               --injector-ipns <DB_IPNS> \
               --injector-ep <IP:PORT>

Now [modify the settings of your
browser](http://www.wikihow.com/Enter-Proxy-Settings-in-Firefox) to:

* Make the client - which runs on port localhost:8080 - it's proxy, AND
* **make sure 'localhost' is not listed in the `"No Proxy for"` field**, AND
* the `"Use this proxy for all protocols"` is checked (mostly for SSL).

Once done, you can enter `localhost` into your browser and it should show you
what database of sites the client is currently using.

It is likely that at first the database shall be `nill` which indicates that
no database has been dowloaded from IPFS yet. This may take from a couple of
seconds up to about three minutes. The page refreshes itself regurarly so
once the client downloads the database, it should display automatically.

In the mean time, notice also the small form at the top of the page looking
something like this:

    Injector proxy: enabled [disable]

This means that proxing to injector is currently `enabled`, which in turn
means that if one points the browser to a non secure http page and the page
isn't yet in the IPFS database, then the client shall forward the HTTP
request to the injector. On success, injector will (A) send the content
back and (B) upload the content to the IPFS database.

Each time the injector updates the database it prints out a message:

    Published DB: Qm...

Once published, it will take some time for the client to download it
(up to three minutes from experience) and once it does so, it will be shown
on client's frontend.

At that point one can disable the proxying through injector, clear
browser's cached data and try to point the browser to the same non secured
HTTP page.

### Integration tests

You can run the integration tests as follows

    $ ./scripts/run_integration_tests.sh

## Using Docker containers

Ouinet injectors and clients can be run as Docker containers.  An application
configuration file for Docker Compose is included for easily deploying all
needed volumes and containers.

To run a Ouinet node container only a couple hundred MiB are needed, plus the
space devoted to the data volume (which may grow considerably larger in the
case of the injector).

A `Dockerfile` is also included that can be used to create a Docker image
which contains the Ouinet injector, client and necessary software dependencies
running on top of a Debian base system.

### Building the image

Ouinet Docker images should be available from the Docker Hub.  Follow the
instructions in this section if you still want to build the image yourself.
You will need around 3 GiB of disk space.

You may use the `Dockerfile` as included in Ouinet's source code, or you
can just [download it][Dockerfile].  Then build the image by running:

    $ sudo docker build -t equalitie/ouinet:latest .

That command will build a default recommended version, which you can override
with `--build-arg OUINET_VERSION=<VERSION>`.

After a while you will get the `equalitie/ouinet:latest` image.  Then you may
want to run `sudo docker prune` to free up the space taken by temporary
builder images (which may amount to a couple of GiB).

[Dockerfile]: https://raw.githubusercontent.com/equalitie/ouinet/master/Dockerfile

### Deploying a client

You may use [Docker Compose](https://docs.docker.com/compose/) with the
`docker-compose.yml` file included in Ouinet's source code (or you can just
[download it][docker-compose.yml]).  If you just plan to run a single client
with the latest code on you computer, you should be fine with:

    $ sudo docker-compose up

That command will create a *data volume*, a main *node container* for running
the Ouinet client or injector (using the host's network directly), and a
convenience *shell container* to allow you to modify files in the data volume,
then run the containers (the shell container will exit immediately; this is
normal).

To stop the node, hit Ctrl+C.  Run `sudo docker-compose images` to see the
names of the actual node and shell containers.

[docker-compose.yml]: https://raw.githubusercontent.com/equalitie/ouinet/master/docker-compose.yml

### Other deployments

If you plan on running several nodes on the same host you will need to use
different explicit Docker Compose project names for them.  To make the node an
injector instead of a client you need to set `OUINET_ROLE=injector`.  To make
the container use a particular image version instead of `latest`, set
`OUINET_VERSION`.

An easy way to set all these parameters is to copy or link the
`docker-compose.yml` file to a directory with the desired project name and
populate its default environment file:

    $ mkdir -p /path/to/ouinet-injector  # ouinet-injector is the project name
    $ cd /path/to/ouinet-injector
    $ cp /path/to/docker-compose.yml .
    $ echo OUINET_ROLE=injector >> .env
    $ echo OUINET_VERSION=v0.0.5-docker3 >> .env
    $ docker-compose up

### Accessing data files

You may use the convenience *shell container* to access Ouinet node data
directly:

    $ sudo docker-compose run --rm shell

This will create a throwaway container with a shell at the `/var/opt/ouinet`
directory in the data volume.

A *new client node* which starts with no configuration in `/var/opt/ouinet`
will get a default one from templates included in Ouinet's source code, and it
will be missing some important parameters, so you may want to stop it and use
the shell container to edit `client/ouinet-client.conf` and add configuration
options for the injector endpoint `injector-ep` and cache IPNS
`injector-ipns`, then restart the client.

If the *injector or client crashes* for some reason, you may have to remove
its PID file manually for it to start again.  Just use the shell container to
remove `injector/pid` or `client/pid`.

If you want to *transfer an existing repository* to `/var/opt/ouinet`, you
first need to move away or remove the existing one using the shell container:

    # mv REPO REPO.old  # REPO is either 'injector' or 'client'

Then you may copy it in from the host using:

    $ sudo docker cp /path/to/REPO SHELL_CONTAINER:/var/opt/ouinet/REPO

### Injector container

After an injector has finished starting, you may want to use the shell
container to inspect and note down the contents of `injector/endpoint-*`
(injector endpoints) and `injector/cache-ipns` (cache IPNS) to be used by
clients.

If you ever need to reset and empty the injector's database for some reason
(e.g. testing) while keeping injector IDs and credentials, you may:

 1. Fetch a Go IPFS binary and copy it to the data volume:

        $ wget "https://dist.ipfs.io/go-ipfs/v0.4.14/go-ipfs_v0.4.14_linux-amd64.tar.gz"
        $ tar -xf go-ipfs_v0.4.14_linux-amd64.tar.gz
        $ sudo docker cp go-ipfs/ipfs SHELL_CONTAINER:/var/opt/ouinet

 2. Stop the injector.
 3. Run a temporary Debian container with access to the data volume:

        $ sudo docker run --rm -it -v ouinet-injector_data:/mnt debian

 4. In the container, run:

        # cd /mnt
        # rm injector/ipfs/ipfs_cache_db.*
        # alias ipfs='./ipfs -Lc injector/ipfs'
        # ipfs pin ls --type recursive | cut -d' ' -f1 | xargs ipfs pin rm
        # ipfs repo gc
        # exit

 5. Start the injector.

## Android

### Requirements

A lot of free space (something less than 15GB). Everything else shall be
downloaded by the `build-android.sh` script.

The instructions below use Vagrant for bulding, but the `build-android.sh`
script should work on any reasonably up-to-date debian based system.

In the following instructions, I'll use `<ANDROID>` to represent the absolute
path to your build directory. That is, the directory from which you'll run the
`build-android.sh` script (e.g. `~/ouinet.android.build`).

### Building

    host    $ vagrant up --provider=libvirt
    host    $ vagrant ssh
    vagrant $ mkdir <ANDROID>
    vagrant $ cd <ANDROID>
    vagrant $ git clone --recursive /vagrant
    vagrant $ ./vagrant/scripts/build-android.sh

When the `build-android.sh` script finishes successfully, it prints out a path
to the `browser-debug.apk` app package which can now be deployed.

Note that above we cloned a fresh copy of the `ouinet` repository. This is not
strictly necessary since the build environment supports out-of-source builds,
however it spares you from having to keep your source directory clean and
submodules up to date. If you fullfill these requirements, you can just skip
the cloning and run `/vagrant/scripts/build-android.sh` instead.

The script builds by default an APK for the `armeabi-v7a` [Android ABI][]. If
you want a build for a different ABI, just set the `ABI` environment variable:

    $ env ABI=x86_64 /path/to/build-android.sh

Please note that merging different ABI builds at the same build directory is
not yet supported.  To remove potentially conflicting files while keeping
downloads and ABI-neutral source files so that you can reuse them for the next
build, please run:

    $ /path/to/build-android.sh abiclean

[Android ABI]: https://developer.android.com/ndk/guides/abis.html

### Setting up Android browser/client

The android app has user menus for specifying the injector's endpoint,
injector's credentials, and the IPFS database ID (IPNS), but that is very
tedious to do. Because of that, the app also gives you an option to
read QR codes. To generate one, create a text with these values:

    injector=<INJECTOR ENDPOINT>
    ipns=<INJECTOR's IPFS-ID/IPNS>
    credentials=<USERNAME:PASSWORD>

and the use an online QR code generator such as [this one](https://www.the-qrcode-generator.com/),
or use a command line tool such as [qrencode](https://fukuchi.org/works/qrencode/)

    $ qrencode -t PNG -o FILE.png < FILE.txt

### Testing with Android emulator

You may also use the `build-android.sh` script to fire up an Android emulator
session with a compatible system image; just run:

    $ /path/to/build-android.sh emu

It will download the necessary files to the current directory (or reuse files
downloaded by the build process, if available) and start the emulator.  Please
note that downloading the system image may take a few minutes, and booting the
emulator for the first time may take more than 10 minutes.  In subsequent
runs, the emulator will just recover the snapshot saved on last quit, which is
way faster.

The `ABI` environment variable described above also works for selecting the
emulator architecture:

    $ env ABI=x86_64 /path/to/build-android.sh emu

You may pass options to the emulator at the script's command line, after a
`--` (double dash) argument.  For instance:

    $ /path/to/build-android.sh emu -- -no-snapshot-save

Some useful options include `-no-snapshot`, `-no-snapshot-load` and
`-no-snapshot-save`.  See [emulator startup options][] for more information.

[emulator startup options]: https://developer.android.com/studio/run/emulator-commandline.html#startup-options

While the emulator is running, you may interact with it using ADB, e.g. to
install the APK built in the previous step.  See the script's output for
particular instructions and paths.
