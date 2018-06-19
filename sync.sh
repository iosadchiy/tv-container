rsync ./container/ tv:./container/ -r

inotifywait -r -m -e close_write --format '%w%f' ./ | while read MODFILE
do
  rsync ./container/ tv:./container/ -r
done
