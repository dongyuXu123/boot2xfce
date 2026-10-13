# 顶层 Makefile：一次跑通所有阶段
STAGES := stages/s0-mbr stages/s1-grub stages/s2-kernel-core stages/s3-enrich stages/s4-desktop

.PHONY: all test test-all clean env
all test clean:
	@set -e; for d in $(STAGES); do $(MAKE) -C $$d $@; done

# 全量回归：任何阶段失败即停（进阶: 前一阶段不过不许过下一阶段）
test-all: env test
	@echo "===== ALL STAGES PASS ====="

env:
	@bash scripts/check-env.sh > /dev/null
