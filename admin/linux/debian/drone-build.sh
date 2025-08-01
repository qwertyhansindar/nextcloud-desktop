#!/bin/bash

# SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

set -xe
shopt -s extglob

env

PPA=ppa:nextcloud-devs/client
PPA_ALPHA=ppa:nextcloud-devs/client-alpha
PPA_BETA=ppa:nextcloud-devs/client-beta

OBS_PROJECT=home:ivaradi
OBS_PROJECT_ALPHA=home:ivaradi:alpha
OBS_PROJECT_BETA=home:ivaradi:beta
OBS_PACKAGE=nextcloud-desktop

if test "${DRONE_TARGET_BRANCH}" = "stable-2.6"; then
    UBUNTU_DISTRIBUTIONS="bionic focal jammy kinetic"
    DEBIAN_DISTRIBUTIONS="buster stretch testing"
else
    UBUNTU_DISTRIBUTIONS="jammy noble plucky questing"
    DEBIAN_DISTRIBUTIONS="bullseye bookworm testing"
fi

declare -A DIST_TO_OBS=(
    ["bullseye"]="Debian_11"
    ["bookworm"]="Debian_12"
    ["testing"]="Debian_Testing"
)

pull_request=${DRONE_PULL_REQUEST:=master}

if test -z "${DRONE_WORKSPACE}"; then
    DRONE_WORKSPACE=`pwd`
fi

if test -z "${DRONE_DIR}"; then
    DRONE_DIR=`dirname ${DRONE_WORKSPACE}`
fi

set +x
if test "$DEBIAN_SECRET_KEY" -a "$DEBIAN_SECRET_IV"; then
    openssl aes-256-cbc -K $DEBIAN_SECRET_KEY -iv $DEBIAN_SECRET_IV -in admin/linux/debian/signing-key.txt.enc -d | gpg --import

    openssl aes-256-cbc -K $DEBIAN_SECRET_KEY -iv $DEBIAN_SECRET_IV -in admin/linux/debian/oscrc.enc -out ~/.oscrc -d

    touch ~/.has_ppa_keys
fi
set -x

cd "${DRONE_WORKSPACE}"
git config --global user.email "drone@noemail.invalid"
git config --global user.name "Drone User"
git fetch --tags

for distribution in ${UBUNTU_DISTRIBUTIONS} ${DEBIAN_DISTRIBUTIONS}; do
    git fetch origin debian/dist/${distribution}/${DRONE_TARGET_BRANCH}
    git checkout origin/debian/dist/${distribution}/${DRONE_TARGET_BRANCH}

    git merge ${DRONE_COMMIT}

    read basever revdate kind <<<$(admin/linux/debian/scripts/git2changelog.py /tmp/tmpchangelog stable "" "" ${DRONE_COMMIT})
    break
done

cd "${DRONE_DIR}"

echo "$kind" > kind

if test "$kind" = "alpha"; then
    repo=nextcloud-devs/client-alpha
elif test "$kind" = "beta"; then
    repo=nextcloud-devs/client-beta
else
    repo=nextcloud-devs/client
fi

origsourceopt=""

cp -a ${DRONE_WORKSPACE} nextcloud-desktop_${basever}-${revdate}
tar cjf nextcloud-desktop_${basever}-${revdate}.orig.tar.bz2 --exclude .git --exclude binary nextcloud-desktop_${basever}-${revdate}

cd "${DRONE_WORKSPACE}"

for distribution in ${UBUNTU_DISTRIBUTIONS} ${DEBIAN_DISTRIBUTIONS}; do
    git checkout -- .
    git clean -xdf

    git fetch origin debian/dist/${distribution}/${DRONE_TARGET_BRANCH}
    git checkout origin/debian/dist/${distribution}/${DRONE_TARGET_BRANCH}

    git merge ${DRONE_COMMIT}

    admin/linux/debian/scripts/git2changelog.py /tmp/tmpchangelog ${distribution} ${revdate} ${basever}
    cat /tmp/tmpchangelog debian/changelog > debian/changelog.new
    mv debian/changelog.new debian/changelog

    fullver=`head -1 debian/changelog | sed "s:nextcloud-desktop (\([^)]*\)).*:\1:"`

    EDITOR=true dpkg-source --commit . local-changes

    dpkg-source --build .
    dpkg-genchanges -S -sa > "../nextcloud-desktop_${fullver}_source.changes"

    if test -f ~/.has_ppa_keys; then
        debsign -k2265D8767D14AA7B -S
    fi
done
cd ..
ls -al

if test "${pull_request}" = "master"; then
    if test "$kind" = "alpha"; then
        PPA=$PPA_ALPHA
        OBS_PROJECT=$OBS_PROJECT_ALPHA
    elif test "$kind" = "beta"; then
        PPA=$PPA_BETA
        OBS_PROJECT=$OBS_PROJECT_BETA
    fi

    if test -f ~/.has_ppa_keys; then
        for distribution in ${UBUNTU_DISTRIBUTIONS}; do
            changes=$(ls -1 nextcloud-desktop_*~${distribution}1_source.changes)
            if test -f "${changes}"; then
                dput $PPA "${changes}" > /dev/null
            fi
        done

        if test -n "${DEBIAN_DISTRIBUTIONS}"; then
            package="nextcloud-desktop"
            OBS_SUBDIR="${OBS_PROJECT}/${package}"

            mkdir -p osc
            pushd osc
            osc co "${OBS_PROJECT}" "${package}"

            if test "$(ls ${OBS_SUBDIR})"; then
                osc delete ${OBS_SUBDIR}/*
            fi

            ln ../nextcloud-desktop*.orig.tar.* ${OBS_SUBDIR}/

            for distribution in ${DEBIAN_DISTRIBUTIONS}; do
                pkgsuffix=".${distribution}"
                pkgvertag="~${distribution}1"
                obs_dist="${DIST_TO_OBS[${distribution}]}"

                ln ../nextcloud-desktop_*[0-9.][0-9]${pkgvertag}.dsc ${OBS_SUBDIR}/nextcloud-desktop-${obs_dist}.dsc
                ln ../nextcloud-desktop_*[0-9.][0-9]${pkgvertag}.debian.tar* ${OBS_SUBDIR}/
            done

            osc add ${OBS_SUBDIR}/*

            cd ${OBS_SUBDIR}
            osc commit -m "Drone update"
            popd
        fi
    fi
fi
