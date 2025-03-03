#!/bin/bash
# Copyright (C) 2022-2025 Fredrik Öhrström (gpl-3.0-or-later)

# Grab all text up to the "<date>: Version x.y.z-RC*" line
# There should be no text.
CHANGES=$(sed '/: Version /q' CHANGES | grep -v ": Version" | sed '/./,$!d' | \
          tac | sed -e '/./,$!d' | tac | sed -e '/./,$!d' > /tmp/release_changes)

if [ -s /tmp/release_changes ]
then
    echo "Oups! There are changes declared in the CHANGES file. There should not be if you are going to deploy."
    exit 0
fi

THIS_VERSION=$(sed '/: Version /q' CHANGES  | grep -o 'Version [0-9]\+\.[0-9]\+\.[0-9]\+')

# Grab all text between the Version RC and the previous VERSION.
sed -n '/: Version [0-9]\.[0-9]\.[0-9]$/q;p' CHANGES | grep -v ": Version "> /tmp/release_changes

if [ ! -s /tmp/release_changes ]
then
    echo "Oups! There should be changes declared in the CHANGES file between the RC version and the previous released version."
    exit 0
fi

cp /tmp/release_changes RELEASE

# Grab the line from CHANGES which says: Version 1.2.3-RC1 2022-12-22
OLD_MESSAGE=$(grep -m 1 ": Version" CHANGES | cut -f 2 -d :)

# Now extract the major, minor and patch.
PARTS=$(echo "$OLD_MESSAGE" | sed 's/ Version \([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\).*/\1 \2 \3/')

MAJOR=$(echo "$PARTS" | cut -f 1 -d ' ')
MINOR=$(echo "$PARTS" | cut -f 2 -d ' ')
PATCH=$(echo "$PARTS" | cut -f 3 -d ' ')

NEW_VERSION="$MAJOR.$MINOR.$PATCH"

if git tag | grep -q "^${NEW_VERSION}\$"
then
    echo "Oups! The new version tag $NEW_VERSION already exists!"
    exit 0
fi

NEW_MESSAGE="$(date +'%Y-%m-%d'): Version $NEW_VERSION"

echo
echo "Deploying >>$NEW_MESSAGE<< with changelog:"
echo "----------------------------------------------------------------------------------"
cat /tmp/release_changes
echo "----------------------------------------------------------------------------------"
echo
while true; do
    read -p "Ok to deploy release? y/n " yn
    case $yn in
        [Yy]* ) break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
    esac
done

# Update the CHANGES file
CMD="1i $NEW_MESSAGE"
sed -i "$CMD" CHANGES
echo "Added version string in CHANGES"

make dist VERSION=$NEW_VERSION

git commit -am "$NEW_MESSAGE"

git tag "$NEW_VERSION"

echo "Now do: git push ; git push --tags"
