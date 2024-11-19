#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usbd_msc.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/fs.h>
#include <ff.h>

LOG_MODULE_REGISTER(bootsel);

#define SW0_NODE DT_ALIAS(sw0)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

#define MOUNTPOINT   "/NAND:"
#define FILENAME     "BOOTSEL"
#define FILELOCATION MOUNTPOINT "/" FILENAME

#define STORAGE_PARTITION    storage_partition
#define STORAGE_PARTITION_ID FIXED_PARTITION_ID(STORAGE_PARTITION)

static bool toggle_state = false;

static int setup_disk(void)
{
	static struct fs_mount_t mp;
	static FATFS fat_fs;
	const struct flash_area *pfa;
	int ret;

	mp.storage_dev = (void *)STORAGE_PARTITION_ID;
	ret = flash_area_open(STORAGE_PARTITION_ID, &pfa);
	if (ret != 0) {
		flash_area_close(pfa);
		LOG_ERR("Error %d: failed to setup flash area", ret);
		return ret;
	}

	LOG_INF("Area %u at 0x%x on %s for %u bytes", STORAGE_PARTITION_ID,
		(unsigned int)pfa->fa_off, pfa->fa_dev->name, (unsigned int)pfa->fa_size);

	mp.type = FS_FATFS;
	mp.fs_data = &fat_fs;
	mp.mnt_point = MOUNTPOINT;
	ret = fs_mount(&mp);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to mount filesystem", ret);
		return ret;
	}

	return 0;
}

static int read_state_from_file(void)
{
	struct fs_file_t file;
	char buffer[2] = {0};
	int ret;

	fs_file_t_init(&file);
	ret = fs_open(&file, FILELOCATION, FS_O_READ);
	if (ret == -ENOENT) {
		LOG_INF("Error %d: file '%s' not found", ret, FILELOCATION);
		return ret;
	}

	ret = fs_read(&file, buffer, sizeof(buffer) - 1);
	if (ret < 0) {
		LOG_ERR("Error %d: failed to read from file", ret);
		fs_close(&file);
		return ret;
	}

	toggle_state = (buffer[0] == '1') ? true : false;
	fs_close(&file);
	return 0;
}

static void write_state_to_file(void)
{
	struct fs_file_t file;
	char data = toggle_state ? '1' : '0';
	int ret;

	fs_file_t_init(&file);
	ret = fs_open(&file, FILELOCATION, FS_O_CREATE | FS_O_WRITE);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to open file for writing", ret);
		return;
	}

	ret = fs_write(&file, &data, 1);
	if (ret < 0) {
		LOG_ERR("Error %d: failed to write to file", ret);
	}

	LOG_INF("Wrote '%c' to file '%s'", data, FILELOCATION);
	fs_close(&file);
}

static void update_led()
{
	int led0_state = toggle_state ? 1 : 0;
	gpio_pin_set_dt(&led0, led0_state);
	gpio_pin_set_dt(&led1, !led0_state);
}

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	static int64_t last_press = 0;
	int64_t now = k_uptime_get();

	if (now - last_press < 500) {
		return;
	}

	last_press = now;
	toggle_state = !toggle_state;

	write_state_to_file();
	update_led();
}

static int configure_led(struct gpio_dt_spec led)
{
	int ret;

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("Error: LED GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
	if (ret < 0) {
		LOG_ERR("Error %d: failed to configure LED GPIO output", ret);
		return ret;
	}

	LOG_INF("Set up LED at %s pin %d", led.port->name, led.pin);
	return 0;
}

static int configure_button(struct gpio_dt_spec button)
{
	int ret;

	if (!gpio_is_ready_dt(&button)) {
		LOG_ERR("Error: failed to configure button (%s) GPIO", button.port->name);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure %s pin %d", ret, button.port->name,
			button.pin);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", ret,
			button.port->name, button.pin);
		return ret;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	ret = gpio_add_callback(button.port, &button_cb_data);
	if (ret != 0) {
		LOG_ERR("Error %d: failed to register button GPIO callback", ret);
		return ret;
	}

	LOG_INF("Set up button at %s pin %d", button.port->name, button.pin);
	return 0;
}

static int configure_gpio(void)
{
	int ret;

	ret = configure_button(button);
	if (ret != 0) {
		LOG_ERR("Failed to configure button: %d", ret);
		return ret;
	}

	ret = configure_led(led0);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED 0: %d", ret);
		return ret;
	}

	ret = configure_led(led1);
	if (ret < 0) {
		LOG_ERR("Failed to configure LED 1: %d", ret);
		return ret;
	}

	LOG_INF("GPIO configured successfully");
	return 0;
}

int main(void)
{
	int ret;

	ret = setup_disk();
	if (ret < 0) {
		LOG_ERR("Failed to initialize NAND: %d", ret);
		return ret;
	}

	ret = configure_gpio();
	if (ret < 0) {
		LOG_ERR("Failed to initialize GPIO: %d", ret);
		return ret;
	}

	ret = read_state_from_file();
	if (ret == -ENOENT) {
		toggle_state = false;
		LOG_WRN("Failed to read " FILENAME "! Assuming toggle_state: false");
	}

	LOG_INF("Boot toggle state: %d", toggle_state);
	update_led();

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return ret;
	}

	LOG_INF("The device is put in USB mass storage mode.");
	return 0;
}
