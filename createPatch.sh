rm -rf *.patch
mkdir temp_diff
mkdir temp_diff/drivers
mkdir temp_diff/drivers/media
mkdir temp_diff/drivers/media/platform
mkdir temp_diff/drivers/media/platform/mxc
mkdir temp_diff/drivers/media/platform/mxc/capture
mkdir temp_diff/drivers/media/v4l2-core
mkdir temp_diff/include
mkdir temp_diff/include/uapi
mkdir temp_diff/include/uapi/linux

cp ./mxc_v4l2_capture.c.original temp_diff/drivers/media/platform/mxc/capture/mxc_v4l2_capture.c
cp ./mxc_v4l2_capture.h.original temp_diff/drivers/media/platform/mxc/capture/mxc_v4l2_capture.h
cp ./videodev2.h.original temp_diff/include/uapi/linux/videodev2.h
cp ./v4l2-compat-ioctl32.c.original temp_diff/drivers/media/v4l2-core/v4l2-compat-ioctl32.c

cd temp_diff
git init
git add drivers/media/platform/mxc/capture/mxc_v4l2_capture.c
git add drivers/media/platform/mxc/capture/mxc_v4l2_capture.h
git add include/uapi/linux/videodev2.h
git add drivers/media/v4l2-core/v4l2-compat-ioctl32.c

git commit -a -m"Original Files"

cp ../mxc_v4l2_capture.c ./drivers/media/platform/mxc/capture/mxc_v4l2_capture.c
cp ../mxc_v4l2_capture.h ./drivers/media/platform/mxc/capture/mxc_v4l2_capture.h
cp ../videodev2.h ./include/uapi/linux/videodev2.h
cp ../v4l2-compat-ioctl32.c ./drivers/media/v4l2-core/v4l2-compat-ioctl32.c

git commit -a -m"v4l2-modifications"

git format-patch -1 -o ../
cd ..
rm -rf ./temp_diff
mv -v 0001-v4l2-modifications.patch /workspace/aranz-yocto/aranz-src/meta-aranz-imx6/recipes-kernel/linux/linux-aranz-imx6-3.14.1.0/silhouettestar-m2/
