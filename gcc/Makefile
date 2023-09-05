TARGET = mh1903_gcc

MCU  = cortex-m4

BIN = bin
OBJ = obj


OUTPUT =$(BIN)/$(TARGET)

LIBDIR = ../mh1903_lib
CODEDIR = ..

#Compiler & Linker
CROSS_COMPILE = arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
AS = $(CROSS_COMPILE)as
AR = $(CROSS_COMPILE)ar
LD = $(CROSS_COMPILE)ld
OBJDUMP = $(CROSS_COMPILE)objdump
OBJCOPY = $(CROSS_COMPILE)objcopy
STRIP = $(CROSS_COMPILE)strip

#CFLAGS
ARCH_FLAGS = -mcpu=$(MCU) -mthumb -mlittle-endian
DEFS = -DUSE_STDPERIPH_DRIVER -DUSE_FULL_ASSERT -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -DARM_MATH_CM4 -D__FPU_PRESENT="1" -D__TARGET_FPU_VFP

CFLAGS = $(ARCH_FLAGS) $(DEFS) $(INCDIR) -g -Os -ffunction-sections -fdata-sections -mfloat-abi=hard -mfpu=fpv4-sp-d16

#LFLAGS
LFLAGS = -T mh1903b.ld -Wl,-Map=$(OUTPUT).map -lm -mcpu=$(MCU) -mthumb -Wl,--gc-sections -nostartfiles -std=gnu99 --specs=nosys.specs -mfloat-abi=hard -mfpu=fpv4-sp-d16

#LIBS
LIBS += $(LIBDIR)/MHSCPU_Driver/lib/MH1903_QRDecodeLib.a

#DIR
APPDIR = $(CODEDIR)/app
COREDIR = $(CODEDIR)/core
DRVDIR = $(CODEDIR)/drv
FREERTOSDIR = $(CODEDIR)/FreeRTOS/Source:\
			$(CODEDIR)/FreeRTOS:\
			$(CODEDIR)/FreeRTOS/CMSIS_RTOS_V2:\
			$(CODEDIR)/FreeRTOS/Source/portable/GCC/ARM_CM4F:\
			$(CODEDIR)/FreeRTOS/Source/portable/MemMang
MIDDIR = $(CODEDIR)/mid/log:\
			$(CODEDIR)/mid/msg:\
			$(CODEDIR)/mid/ram:\
			$(CODEDIR)/mid/fatfs:\
			$(CODEDIR)/mid/param:\
			$(CODEDIR)/mid/cbor:\
			$(CODEDIR)/mid/cbor/tinycbor:\
			$(CODEDIR)/mid/utils:\
			$(CODEDIR)/mid/param/cjson:\
			$(CODEDIR)/mid/checksum
TASKSDIR = $(CODEDIR)/tasks
CMBACKTRACEDIR = $(CODEDIR)/cm_backtrace
LVGLDIR = $(CODEDIR)/lvgl/src/core:\
			$(CODEDIR)/lvgl/src/draw:\
			$(CODEDIR)/lvgl/src/draw/sw:\
			$(CODEDIR)/lvgl/src/extra:\
			$(CODEDIR)/lvgl/src/extra/layouts/flex:\
			$(CODEDIR)/lvgl/src/extra/layouts/grid:\
			$(CODEDIR)/lvgl/src/extra/themes/basic:\
			$(CODEDIR)/lvgl/src/extra/themes/default:\
			$(CODEDIR)/lvgl/src/extra/themes/mono:\
			$(CODEDIR)/lvgl/src/extra/widgets/animimg:\
			$(CODEDIR)/lvgl/src/extra/widgets/calendar:\
			$(CODEDIR)/lvgl/src/extra/widgets/chart:\
			$(CODEDIR)/lvgl/src/extra/widgets/colorwheel:\
			$(CODEDIR)/lvgl/src/extra/widgets/imgbtn:\
			$(CODEDIR)/lvgl/src/extra/widgets/keyboard:\
			$(CODEDIR)/lvgl/src/extra/widgets/led:\
			$(CODEDIR)/lvgl/src/extra/widgets/list:\
			$(CODEDIR)/lvgl/src/extra/widgets/menu:\
			$(CODEDIR)/lvgl/src/extra/widgets/meter:\
			$(CODEDIR)/lvgl/src/extra/widgets/msgbox:\
			$(CODEDIR)/lvgl/src/extra/widgets/span:\
			$(CODEDIR)/lvgl/src/extra/widgets/spinbox:\
			$(CODEDIR)/lvgl/src/extra/widgets/spinner:\
			$(CODEDIR)/lvgl/src/extra/widgets/tabview:\
			$(CODEDIR)/lvgl/src/extra/widgets/tileview:\
			$(CODEDIR)/lvgl/src/extra/widgets/win:\
			$(CODEDIR)/lvgl/src/extra/libs/qrcode:\
			$(CODEDIR)/lvgl/src/font:\
			$(CODEDIR)/lvgl/src/hal:\
			$(CODEDIR)/lvgl/src/misc:\
			$(CODEDIR)/lvgl/src/widgets
LVGLDEMODIR = $(CODEDIR)/ui/lv_demos/widgets:\
			$(CODEDIR)/ui/lv_demos/widgets/assets:\
			$(CODEDIR)/ui/lv_demos/benchmark:\
			$(CODEDIR)/ui/lv_demos/benchmark/assets
PILLAR_DEMOSDIR = $(CODEDIR)/ui/pillar_demos/qrcode:\
			$(CODEDIR)/ui/pillar_demos/qrcode_sign
CRYPTIONDIR = $(CODEDIR)/cryption:\
			$(CODEDIR)/cryption/ur:\
			$(CODEDIR)/cryption/secp256k1


VPATH = $(wildcard $(LIBDIR)/*) $(wildcard $(LIBDIR)/*/*) $(wildcard $(LIBDIR)/*/*/*):\
		$(wildcard $(LIBDIR)/*/*/*/*) $(LIBDIR)/Device/MegaHunt/mhscpu/Source/GCC:\
		$(APPDIR):\
		$(COREDIR):\
		$(DRVDIR):\
		$(FREERTOSDIR):\
		$(MIDDIR):\
		$(TASKSDIR):\
		$(CMBACKTRACEDIR):\
		$(CMBACKTRACEDIR)/fault_handler/gcc:\
		$(LVGLDIR):\
		$(LVGLDEMODIR):\
		$(PILLAR_DEMOSDIR):\
		$(CRYPTIONDIR):\
		

INCDIR += -I$(LIBDIR)/MHSCPU_Driver/inc
INCDIR += -I$(LIBDIR)/CMSIS/Include
INCDIR += -I$(LIBDIR)/Device/MegaHunt/mhscpu/Include
INCDIR += -I$(CODEDIR)/core
INCDIR += -I$(CODEDIR)/app
INCDIR += -I$(CODEDIR)/mid/ram
INCDIR += -I$(CODEDIR)/mid/msg
INCDIR += -I$(CODEDIR)/mid/log
INCDIR += -I$(CODEDIR)/mid/fatfs
INCDIR += -I$(CODEDIR)/mid/param
INCDIR += -I$(CODEDIR)/mid/checksum
INCDIR += -I$(CODEDIR)/mid/cbor
INCDIR += -I$(CODEDIR)/mid/utils
INCDIR += -I$(CODEDIR)/tasks
INCDIR += -I$(CODEDIR)/drv
INCDIR += -I$(CODEDIR)/FreeRTOS/Source/include
INCDIR += -I$(CODEDIR)/FreeRTOS
INCDIR += -I$(CODEDIR)/FreeRTOS/Source/portable/GCC/ARM_CM4F
INCDIR += -I$(CODEDIR)/FreeRTOS/CMSIS_RTOS_V2
INCDIR += -I$(CODEDIR)/cm_backtrace
INCDIR += -I$(CODEDIR)/lvgl
INCDIR += -I$(CODEDIR)/lvgl/src
INCDIR += -I$(CODEDIR)/lvgl/src/core
INCDIR += -I$(CODEDIR)/lvgl/src/draw
INCDIR += -I$(CODEDIR)/lvgl/src/extra
INCDIR += -I$(CODEDIR)/lvgl/src/font
INCDIR += -I$(CODEDIR)/lvgl/src/hal
INCDIR += -I$(CODEDIR)/lvgl/src/misc
INCDIR += -I$(CODEDIR)/lvgl/src/widgets
INCDIR += -I$(CODEDIR)/cryption
INCDIR += -I$(CODEDIR)/cryption/ur
INCDIR += -I$(CODEDIR)/cryption/secp256k1
INCDIR += -I$(CODEDIR)/ui/lv_demos
INCDIR += -I$(CODEDIR)/ui/pillar_demos


#SOURCE
ASM_OBJECTS = startup_mhscpu.o cmb_fault.o

C_OBJECTS = main.o
C_OBJECTS += test_cmd.o test_image.o
C_OBJECTS += drv_ili9488.o drv_psram.o drv_spi.o drv_sys.o drv_trng.o drv_uart.o drv_nt35510.o drv_virtual_touch.o drv_ds28s60.o drv_qrdecode.o drv_gd25qxx.o drv_sdcard.o drv_sensor.o
C_OBJECTS += croutine.o event_groups.o list.o queue.o stream_buffer.o tasks.o timers.o heap_4.o port.o cmsis_os2.o rtos_expand.o
C_OBJECTS += log_print.o general_msg.o user_msg.o psram_heap_4.o user_memory.o diskio.o ff.o user_fatfs.o user_param.o cJSON.o crc.o user_utils.o user_cbor.o cborencoder.o cborencoder_close_container_checked.o cborencoder_float.o cborerrorstrings.o cborparser.o cborparser_dup_string.o cborparser_float.o cborpretty.o
C_OBJECTS += cmd_task.o test_task.o ui_display_task.o qrdecode_task.o
C_OBJECTS += misc.o mhscpu_exti.o mhscpu_gpio.o mhscpu_sysctrl.o mhscpu_uart.o mhscpu_spi.o mhscpu_rtc.o mhscpu_dma.o mhscpu_psram.o mhscpu_trng.o mhscpu_lcdi.o mhscpu_dcmi.o mhscpu_i2c.o mhscpu_timer.o mhscpu_sdio.o mhscpu_sensor.o mhscpu_ssc.o
C_OBJECTS += cm_backtrace.o

#lvgl
C_OBJECTS += lv_disp.o lv_event.o lv_group.o lv_indev.o lv_indev_scroll.o lv_obj.o lv_obj_class.o lv_obj_draw.o lv_obj_pos.o lv_obj_scroll.o lv_obj_style.o lv_obj_style_gen.o lv_obj_tree.o lv_refr.o lv_theme.o
C_OBJECTS += lv_draw.o lv_draw_arc.o lv_draw_img.o lv_draw_label.o lv_draw_layer.o lv_draw_line.o lv_draw_mask.o lv_draw_rect.o lv_draw_transform.o lv_draw_triangle.o lv_img_buf.o lv_img_cache.o lv_img_decoder.o
C_OBJECTS += lv_draw_sw.o lv_draw_sw_arc.o lv_draw_sw_blend.o lv_draw_sw_dither.o lv_draw_sw_gradient.o lv_draw_sw_img.o lv_draw_sw_layer.o lv_draw_sw_letter.o lv_draw_sw_line.o lv_draw_sw_polygon.o lv_draw_sw_rect.o lv_draw_sw_transform.o
C_OBJECTS += lv_extra.o lv_flex.o lv_grid.o
C_OBJECTS += lv_theme_basic.o lv_theme_default.o lv_theme_mono.o
C_OBJECTS += lv_animimg.o lv_calendar.o lv_calendar_header_arrow.o lv_calendar_header_dropdown.o lv_chart.o lv_colorwheel.o lv_imgbtn.o lv_keyboard.o lv_led.o lv_list.o lv_menu.o lv_meter.o lv_msgbox.o lv_span.o lv_spinbox.o lv_spinner.o lv_tabview.o lv_tileview.o lv_win.o
C_OBJECTS += lv_font.o lv_font_dejavu_16_persian_hebrew.o lv_font_fmt_txt.o lv_font_loader.o lv_font_montserrat_8.o lv_font_montserrat_10.o lv_font_montserrat_12.o lv_font_montserrat_12_subpx.o lv_font_montserrat_14.o lv_font_montserrat_16.o lv_font_montserrat_18.o lv_font_montserrat_20.o
C_OBJECTS += lv_font_montserrat_22.o lv_font_montserrat_24.o lv_font_montserrat_26.o lv_font_montserrat_28.o lv_font_montserrat_28_compressed.o lv_font_montserrat_30.o lv_font_montserrat_32.o lv_font_montserrat_34.o lv_font_montserrat_36.o lv_font_montserrat_38.o lv_font_montserrat_40.o
C_OBJECTS += lv_font_montserrat_42.o lv_font_montserrat_44.o lv_font_montserrat_46.o lv_font_montserrat_48.o
C_OBJECTS += lv_font_simsun_16_cjk.o lv_font_unscii_8.o lv_font_unscii_16.o
C_OBJECTS += lv_hal_disp.o lv_hal_indev.o lv_hal_tick.o
C_OBJECTS += lv_anim.o lv_anim_timeline.o lv_area.o lv_async.o lv_bidi.o lv_color.o lv_fs.o lv_gc.o lv_ll.o lv_log.o lv_lru.o lv_math.o lv_mem.o lv_printf.o lv_style.o lv_style_gen.o lv_templ.o lv_timer.o lv_tlsf.o lv_txt.o lv_txt_ap.o lv_utils.o
C_OBJECTS += lv_arc.o lv_bar.o lv_btn.o lv_btnmatrix.o lv_canvas.o lv_checkbox.o lv_dropdown.o lv_img.o lv_label.o lv_line.o lv_objx_templ.o lv_roller.o lv_slider.o lv_switch.o lv_table.o lv_textarea.o
C_OBJECTS += lv_qrcode.o qrcodegen.o

#lvgl demo
C_OBJECTS += lv_demo_widgets.o img_clothes.o img_demo_widgets_avatar.o img_lvgl_logo.o
C_OBJECTS += lv_demo_benchmark.o img_benchmark_cogwheel_alpha16.o img_benchmark_cogwheel_argb.o img_benchmark_cogwheel_chroma_keyed.o img_benchmark_cogwheel_indexed16.o img_benchmark_cogwheel_rgb.o img_benchmark_cogwheel_rgb565a8.o lv_font_bechmark_montserrat_12_compr_az.c.o lv_font_bechmark_montserrat_16_compr_az.c.o lv_font_bechmark_montserrat_28_compr_az.c.o
C_OBJECTS += gui_qrcode_example.o ui_qrcode_sign.o

#cryption
C_OBJECTS += sha256.o sha256_hmac.o md5.o bytewords.o ur_decode.o keccak256.o precomputed_ecmult.o precomputed_ecmult_gen.o secp256k1.o user_secp256k1.o ur_eth.o

#C_OBJECTS += xxxxx.o xxxxx.o xxxxx.o xxxxx.o xxxxx.o xxxxx.o xxxxx.o xxxxx.o




ASM_OBJECTS_DDR = $(addprefix $(OBJ)/,$(ASM_OBJECTS))
C_OBJECTS_DDR = $(addprefix $(OBJ)/,$(C_OBJECTS))

#build
all: $(OUTPUT).elf

$(OUTPUT).elf : $(ASM_OBJECTS_DDR) $(C_OBJECTS_DDR)
	$(CC) -o $(OUTPUT).elf $^ $(LIBS) $(LFLAGS)
#$(OBJDUMP) -D -S $(OUTPUT).elf > $(BIN)/asm.txt
	$(OBJCOPY) -O binary $(OUTPUT).elf $(OUTPUT).bin
	$(OBJCOPY) -O ihex $(OUTPUT).elf $(OUTPUT).hex

$(ASM_OBJECTS_DDR) : $(OBJ)/%.o:%.s
	echo $(CC) $<
	$(CC) $(CFLAGS) -c -o $@ $<
		
$(C_OBJECTS_DDR) : $(OBJ)/%.o:%.c
	echo $(CC) $<
	$(CC) $(CFLAGS) -c -o $@ $<
	
#clean
clean:
	-rm -f $(OBJ)/*.* $(BIN)/*.*



