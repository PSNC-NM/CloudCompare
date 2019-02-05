qWrlAnimation
=============

This plugin is used to turn a directory of single frame camera coords (VRML97) into a a directory of images that are suitable for being put into a movie.

To use, create a view (ctrl-V)(IMPORTANT be in viewer-based perspective). Select the view. In the popup dialog, adjust the relative position, camera name, directory containing *.wrl, and a output directory. If you want to preview the movie set framerate and click preview button. Press Render to save a series of images (save with filenames reflecting the time) into the save directory.

The images can be turned into a movie using a program like ffmpeg.

Notes:
- IMPORTANT the *.wrl files should look like this NAME_FRAMENUMBER.wrl |Export_1.wrl etc. Its a file name then "_" then frame number. It cant be "name_01.wrl".
- Splinter required <https://github.com/bgrimstad/splinter/>
- YT example <https://youtu.be/qLQ_7-k3FB8/> 