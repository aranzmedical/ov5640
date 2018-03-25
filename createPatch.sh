rm -rf *.patch
mkdir temp_diff
mkdir temp_diff/drivers
mkdir temp_diff/drivers/media
mkdir temp_diff/drivers/media/platform
mkdir temp_diff/drivers/media/platform/mxc
mkdir temp_diff/drivers/media/platform/mxc/capture
cp mxc_v4l2_capture.c.original temp_diff/drivers/media/platform/mxc/capture/mxc_v4l2_capture.c
cp mxc_v4l2_capture.h.original temp_diff/drivers/media/platform/mxc/capture/mxc_v4l2_capture.h
cd temp_diff
git init
git add drivers/media/platform/mxc/capture/mxc_v4l2_capture.c
git add drivers/media/platform/mxc/capture/mxc_v4l2_capture.h
git commit -a -m"Original Files"

cp ../mxc_v4l2_capture.c ./drivers/media/platform/mxc/capture/mxc_v4l2_capture.c
cp ../mxc_v4l2_capture.h ./drivers/media/platform/mxc/capture/mxc_v4l2_capture.h

git commit -a -m"aranzmodifications"

git format-patch -1 -o ../
cd ..
rm -rf ./temp_diff
mv -v 0001-aranzmodifications.patch /workspace/aranz-yocto/aranz-src/meta-aranz-imx6/recipes-silhouette/camera-driver/linux-aranz-imx6-3.14.1.0/
