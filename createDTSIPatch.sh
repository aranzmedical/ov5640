rm -rf *.patch

mkdir temp_diff
mkdir temp_diff/arch
mkdir temp_diff/arch/arm
mkdir temp_diff/arch/arm/boot
mkdir temp_diff/arch/arm/boot/dts
mkdir temp_diff/arch/arm/mach-imx
mkdir temp_diff/include
mkdir temp_diff/include/dt-bindings
mkdir temp_diff/include/dt-bindings/clock

cp -v ./imx6qdl.dtsi.original temp_diff/arch/arm/boot/dts/imx6qdl.dtsi
cp -v ./clk-imx6q.c.original temp_diff/arch/arm/mach-imx/clk-imx6q.c
cp -v ./imx6qdl-clock.h.original temp_diff/include/dt-bindings/clock/imx6qdl-clock.h

cd temp_diff
git init
git add ./arch/arm/boot/dts/imx6qdl.dtsi
git add ./arch/arm/mach-imx/clk-imx6q.c
git add ./include/dt-bindings/clock/imx6qdl-clock.h

git commit -a -m"Original Files"

cp -v ../imx6qdl.dtsi ./arch/arm/boot/dts/imx6qdl.dtsi
cp -v ../clk-imx6q.c ./arch/arm/mach-imx/clk-imx6q.c
cp -v ../imx6qdl-clock.h ./include/dt-bindings/clock/imx6qdl-clock.h

git add ./arch/arm/boot/dts/imx6qdl.dtsi
git add ./arch/arm/mach-imx/clk-imx6q.c
git add ./include/dt-bindings/clock/imx6qdl-clock.h

git commit -m"Silhouettestar_devicetree"

git format-patch -1 -o ../
cd ..
rm -rf ./temp_diff
mv -v 0001-Silhouettestar_devicetree.patch /workspace/aranz-yocto/aranz-src/meta-aranz-imx6/recipes-kernel/linux/linux-aranz-imx6-3.14.1.0/silhouettestar-m2/
