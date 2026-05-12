# Flatpak packaging

This directory contains a Flatpak manifest for building ngscopeclient as
`org.ngscopeclient.ngscopeclient`.

Build locally from the repository root with:

```sh
flatpak-builder --force-clean --install-deps-from=flathub build-flatpak packaging/flatpak/org.ngscopeclient.ngscopeclient.yml
```

On memory-constrained builders, add `--jobs=2`.

Create a bundle with:

```sh
flatpak-builder --repo=flatpak-repo --force-clean --install-deps-from=flathub build-flatpak packaging/flatpak/org.ngscopeclient.ngscopeclient.yml
flatpak build-bundle flatpak-repo ngscopeclient.flatpak org.ngscopeclient.ngscopeclient
```

The manifest grants network and device access because ngscopeclient controls
test instruments over network, USB, serial, and GPU/Vulkan interfaces.
