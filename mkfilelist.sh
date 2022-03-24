cd ./contents
find . -type f -print | cut -d'/' -f2- | tail -n +2 > .content
cd ..

