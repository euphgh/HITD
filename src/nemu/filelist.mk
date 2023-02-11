#***************************************************************************************
# Copyright (c) 2014-2022 Zihao Yu, Nanjing University
#
# NEMU is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#
# See the Mulan PSL v2 for more details.
#**************************************************************************************/

LIBS += $(if $(CONFIG_NSC_NEMU),-lreadline -ldl -pie,)

SRCS-$(CONFIG_NSC_NEMU) += src/nemu/nemu-main.c
DIRS-$(CONFIG_NSC_NEMU) += src/monitor 
DIRS-y += src/nemu/cpu 
DIRS-y += src/nemu/utils
DIRS-y += src/nemu/memory