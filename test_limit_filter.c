#include <stdio.h>
#include <stdint.h>
#include <string.h>

// 结构体定义（与你的代码一致）
typedef struct {
    uint16_t last_val;
    uint16_t max_step;
    uint16_t pending_val;
    uint8_t  pending_dir;
    uint8_t  confirm_count;
    uint8_t  confirm_n;
} LimitFilter_t;

// 初始化函数（复制原代码）
void util_limit_filter_init(LimitFilter_t *f, uint16_t init_val, uint16_t step, uint8_t n) {
    if (f == NULL) return;
    f->last_val    = init_val;
    f->max_step    = step;
    f->pending_val = 0;
    f->pending_dir = 0;
    f->confirm_count = 0;
    f->confirm_n   = (n > 0) ? n : 1;
}

// 滤波函数（复制原代码）
uint16_t util_limit_filter(LimitFilter_t *f, uint16_t new_val) {
    if (f == NULL) return new_val;

    uint16_t upper = (f->last_val + f->max_step <= 65535) ? (f->last_val + f->max_step) : 65535;
    uint16_t lower = (f->last_val - f->max_step) <= f->last_val ? (f->last_val - f->max_step) : 0;

    if (new_val >= lower && new_val <= upper) {
        f->pending_dir = 0;
        f->confirm_count = 0;
        return f->last_val;
    }

    uint16_t clamped;
    uint8_t  dir;
    if (new_val > upper) {
        clamped = upper;
        dir = 1;
    } else {
        clamped = lower;
        dir = 2;
    }

    if (f->pending_dir == dir) {
        f->confirm_count++;
    } else {
        f->pending_dir = dir;
        f->confirm_count = 1;
    }
    f->pending_val = clamped;

    if (f->confirm_count >= f->confirm_n) {
        f->last_val = clamped;
        f->pending_dir = 0;
        f->confirm_count = 0;
        return clamped;
    } else {
        return f->last_val;
    }
}

// 测试主函数
int main() {
    LimitFilter_t filter;
    // 初始化：起始值=1000，最大步长=50，需要连续3次超限才更新
    util_limit_filter_init(&filter, 1000, 50, 3);

    // 模拟输入数据：正常波动 -> 尖峰干扰 -> 持续上升 -> 持续下降
    uint16_t input[] = {
        1000, 1010, 1005, 990, 1002,   // 范围内的抖动
        1200,                           // 单次尖峰（不应被跟踪）
        1005, 1010,                     // 回到正常
        1100, 1150, 1200, 1250, 1300,   // 持续上升（应逐步限幅输出）
        1350, 1350, 1350, 1350,         // 继续保持高位
        900, 850, 800, 750              // 持续下降
    };
    int len = sizeof(input) / sizeof(input[0]);

    printf("Step = 50, confirm_n = 3\n");
    printf("%-6s %-8s %-10s\n", "Index", "Input", "Output");
    for (int i = 0; i < len; i++) {
        uint16_t out = util_limit_filter(&filter, input[i]);
        printf("%-6d %-8u %-10u\n", i, input[i], out);
    }

    return 0;
}
