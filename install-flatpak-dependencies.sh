#!/bin/bash


git submodule init

git submodule update

flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

flatpak install flathub org.gnome.Builder -y

flatpak install flathub org.freedesktop.Sdk//18.08 -y

flatpak install flathub org.freedesktop.Platform/x86_64/18.08 -y
