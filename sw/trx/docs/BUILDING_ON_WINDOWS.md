## Building on Windows

This guide describes the officially supported Windows build workflow using WSL.

## Installing dependencies

Install WSL and Ubuntu first.

1. Run PowerShell as Administrator.
2. Enable the Windows Subsystem for Linux feature:

    ```powershell
    Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Windows-Subsystem-Linux
    ```

3. Restart the computer.
4. Open the Microsoft Store.
5. Install Ubuntu.

Once WSL is installed, continue by following the Linux build guide from within
your Ubuntu environment.

## Building TRX

After opening Ubuntu in WSL, follow the steps in
[BUILDING_ON_LINUX.md](BUILDING_ON_LINUX.md).

## Other build methods

The WSL workflow above is the recommended way to build TRX on Windows.

If you want to experiment with Visual Studio or other native Windows build
methods, you are welcome to do so, but they are not part of the project's
official build workflow.

## Running the game

Once the game directory is prepared, run TRX from the copied build output and
game files as described in [BUILDING_ON_LINUX.md](BUILDING_ON_LINUX.md).
