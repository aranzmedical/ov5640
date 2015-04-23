#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xd3b69060, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xb4809281, __VMLINUX_SYMBOL_STR(clk_unprepare) },
	{ 0x738e67e6, __VMLINUX_SYMBOL_STR(i2c_master_send) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0x98cc564a, __VMLINUX_SYMBOL_STR(regulator_set_voltage) },
	{ 0xbf2448ae, __VMLINUX_SYMBOL_STR(clk_enable) },
	{ 0x2e5810c6, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr1) },
	{ 0x80ad4480, __VMLINUX_SYMBOL_STR(mipi_csi2_reset) },
	{ 0x4d6b0607, __VMLINUX_SYMBOL_STR(i2c_del_driver) },
	{ 0xbdcdb3f2, __VMLINUX_SYMBOL_STR(mipi_csi2_set_datatype) },
	{ 0x1258d9d9, __VMLINUX_SYMBOL_STR(regulator_disable) },
	{ 0x3f646d7b, __VMLINUX_SYMBOL_STR(clk_disable) },
	{ 0xdac11bae, __VMLINUX_SYMBOL_STR(of_property_read_u32_array) },
	{ 0x432fd7f6, __VMLINUX_SYMBOL_STR(__gpio_set_value) },
	{ 0xe707d823, __VMLINUX_SYMBOL_STR(__aeabi_uidiv) },
	{ 0xfa2a45e, __VMLINUX_SYMBOL_STR(__memzero) },
	{ 0xab8965b7, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x6a0b2cbd, __VMLINUX_SYMBOL_STR(v4l2_int_device_register) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x2e1f4d70, __VMLINUX_SYMBOL_STR(mipi_csi2_enable) },
	{ 0x8e865d3c, __VMLINUX_SYMBOL_STR(arm_delay_ops) },
	{ 0x7bd4efe7, __VMLINUX_SYMBOL_STR(v4l2_int_device_unregister) },
	{ 0x60ec4880, __VMLINUX_SYMBOL_STR(devm_gpio_request_one) },
	{ 0x2196324, __VMLINUX_SYMBOL_STR(__aeabi_idiv) },
	{ 0x76aff186, __VMLINUX_SYMBOL_STR(i2c_register_driver) },
	{ 0xe3fff210, __VMLINUX_SYMBOL_STR(devm_regulator_get) },
	{ 0x6df6dee2, __VMLINUX_SYMBOL_STR(mipi_csi2_dphy_status) },
	{ 0x7b4c34d3, __VMLINUX_SYMBOL_STR(clk_prepare) },
	{ 0xa489abf7, __VMLINUX_SYMBOL_STR(mipi_csi2_disable) },
	{ 0x3757c9b, __VMLINUX_SYMBOL_STR(of_get_named_gpio_flags) },
	{ 0x1d96db61, __VMLINUX_SYMBOL_STR(mipi_csi2_set_lanes) },
	{ 0xabe27502, __VMLINUX_SYMBOL_STR(v4l2_ctrl_query_fill) },
	{ 0xa75b1ab1, __VMLINUX_SYMBOL_STR(devm_clk_get) },
	{ 0xa2d0daf4, __VMLINUX_SYMBOL_STR(i2c_master_recv) },
	{ 0xe04d46e2, __VMLINUX_SYMBOL_STR(mipi_csi2_get_status) },
	{ 0x99161f73, __VMLINUX_SYMBOL_STR(mipi_csi2_get_info) },
	{ 0x9d669763, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0x76c46697, __VMLINUX_SYMBOL_STR(mipi_csi2_get_error1) },
	{ 0x91a7fc85, __VMLINUX_SYMBOL_STR(dev_warn) },
	{ 0xefd6cf06, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr0) },
	{ 0xca16dbe7, __VMLINUX_SYMBOL_STR(regulator_enable) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("i2c:ov5640_mipi");

MODULE_INFO(srcversion, "BACEFAB8F555D26B2753270");
