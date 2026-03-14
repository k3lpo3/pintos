### Running Pintos in Docker (Threads Project)

From the Pintos repo root:

```bash
docker compose build
docker compose run --rm app bash
```

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

### Notes

- `compose.yaml` bind-mounts the current repo into `/work/pintos`, so edits on your host are reflected immediately.
- The image includes both Bochs and QEMU. The threads tests default to Bochs (`SIMULATOR = --bochs` in `src/threads/Make.vars`).

### Deploying your application to the cloud

First, build your image, e.g.: `docker build -t myapp .`.
If your cloud uses a different CPU architecture than your development
machine (e.g., you are on a Mac M1 and your cloud provider is amd64),
you'll want to build the image for that platform, e.g.:
`docker build --platform=linux/amd64 -t myapp .`.

Then, push it to your registry, e.g. `docker push myregistry.com/myapp`.

Consult Docker's [getting started](https://docs.docker.com/go/get-started-sharing/)
docs for more detail on building and pushing.
