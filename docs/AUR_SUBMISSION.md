# AUR Submission Guide for Keypop

This guide explains how to submit `keypop` to the Arch User Repository (AUR).

## Prerequisites
1.  **Arch Linux** installation.
2.  **SSH Key** added to your AUR account settings (Profile > SSH Public Key).
3.  **Packages**: `base-devel`, `git`.

## Step 1: Prepare Your Files
I have already created the `PKGBUILD` for you in the project root.
- **Check `PKGBUILD`**: Verify the `url`, `maintainer` email, and dependencies.
- **License**: Ensure you have a `LICENSE` file in your main repo (e.g., creating a MIT license file).

## Step 2: Generate .SRCINFO
This file is required by the AUR. Run this command in your project root:
```bash
makepkg --printsrcinfo > .SRCINFO
```

## Step 3: Create the AUR Repository
1.  Go to the [AUR Home](https://aur.archlinux.org/) (log in required).
2.  You don't create "repos" via UI. You clone a non-existent url and push to it.
**Package Name**: `keypop-git` (since it builds from git source).

## Step 4: Clone and Push
Open a terminal in a *temporary directory* (outside your main project) to manage the submission repo.

```bash
# 1. Clone the (empty) AUR repo
git clone ssh://aur@aur.archlinux.org/keypop-git.git
cd keypop-git

# 2. Copy your files
cp /path/to/your/project/PKGBUILD .
cp /path/to/your/project/.SRCINFO .
# If you have specific patch files, copy them too.

# 3. Add and Commit
git add PKGBUILD .SRCINFO
git commit -m "Initial release"

# 4. Push to AUR
git push origin master
```

## Step 5: Verify
Once pushed, you can find your package at: `https://aur.archlinux.org/packages/keypop-git`

## Installing with `yay`
After pushing, users can install it using:
```bash
yay -S keypop-git
```

## Updating the Package
When you update your code on GitHub:
1.  Update the `pkgver` or just trigger a rebuild if using `-git` versioning.
2.  Regenerate `.SRCINFO`: `makepkg --printsrcinfo > .SRCINFO`
3.  Commit and push changes to the AUR repo.
