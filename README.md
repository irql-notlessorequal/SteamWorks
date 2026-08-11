# SteamWorks

Exposing SteamWorks functions to SourcePawn.

## How to get a build

Click on the Actions tab above, this will take you to a list of builds.

Choose the latest build from the `universal` branch, as builds older than ninety days have their artifacts automatically removed.

> Make sure it has the "SteamWorks.ext Build" subtext!

Pick the artifact that suits what your server is running on.

Click on the download icon _(down arrow with tray)_ to download the artifact,
if the icon is not visible, you have to sign in to GitHub in order for it to let you download them.

## Forking this repository

Out of the box, GitHub Actions will not work on your fork.

This is because of two reasons.

1. The `SteamworksSDK` repository that the build requires is private, out of concerns
for licensing. You need to provide your own copy.

   You can obtain a copy from https://partner.steamgames.com/?goto=%2Fdownloads%2Flist

   The expected format for the repository is like this:

    ```
    |- sdk
       |- glmgr
       |- public/steam
       |- redistributable_bin
       |- steamworksexample
       |- tools
       |- Readme.txt
    ```

    A repository secret named `STEAMWORKS_SDK_OAUTH` is required, which is a [fine grained personal-access token](https://github.com/settings/personal-access-tokens) that provides repository access[1] to your `SteamworksSDK` copy.

    [1] "Read access to code and metadata" is required

2. If you wish to have build keep-alive functioning, you need to provide the `keep-alive.yml` workflow another repository secret named `PERSONAL_ACCESS_TOKEN` which is a [fine grained PAT](https://github.com/settings/personal-access-tokens) that points to your fork.

   You **MUST** provide the following permissions to the token.

   -  Read access to metadata

   -  Read and Write access to actions