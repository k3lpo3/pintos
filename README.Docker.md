### Running Pintos in Docker (Threads Project)

From the Pintos repo root:

```bash
docker compose build
export UID="$(id -u)" GID="$(id -g)"
docker compose run --rm app bash
```

If you see `The command 'docker' could not be found in this WSL 2 distro`, that is a host setup issue (Docker Desktop WSL integration not enabled, or Docker CLI not installed in that distro). Fix that first, then rerun the commands above.

Inside the container:

```bash
cd src/threads
make clean
make check
```

If you want to run a single test:

```bash
cd src/threads
make clean
PATH="$PATH:/work/pintos/src/utils" pintos --help
PATH="$PATH:/work/pintos/src/utils" make build/alarm-single
```

### Running Pintos in Docker (User Programs Project)

Inside the container:

```bash
cd src/userprog
make clean
make
cd build
make check
```

### Notes

- `compose.yaml` bind-mounts the current repo into `/work/pintos`, so edits on your host are reflected immediately.
- Build artifacts are created under `src/*/build` in the bind-mounted repo. If you previously ran Docker as root and now `make clean` fails, delete the old build directory once on your host (e.g. `sudo rm -rf src/threads/build src/userprog/build`).
- The image includes both Bochs and QEMU, and `make check` will auto-pick a simulator (QEMU preferred) unless you override `SIMULATOR=...`.

### Deploying your application to the cloud

First, build your image, e.g.: `docker build -t myapp .`.
If your cloud uses a different CPU architecture than your development
machine (e.g., you are on a Mac M1 and your cloud provider is amd64),
you'll want to build the image for that platform, e.g.:
`docker build --platform=linux/amd64 -t myapp .`.

Then, push it to your registry, e.g. `docker push myregistry.com/myapp`.

Consult Docker's [getting started](https://docs.docker.com/go/get-started-sharing/)
docs for more detail on building and pushing.
