#include <cstdint>
#include <iostream>
#include <vector>
#include <map>
#include <fstream>
#include <hidapi/hidapi.h>
#include <unistd.h>

const unsigned short MOUSE_VID = 0x1d57;
const unsigned short MOUSE_PID = 0xfa61;

//mouse controller class yes
class MouseController {
private:
    hid_device* handle;
    uint16_t vendor_id;
    uint16_t product_id;
    std::map<std::string, std::vector<uint8_t>> color_presets;

public:


    MouseController(uint16_t vid, uint16_t pid)
        : handle(nullptr), vendor_id(vid), product_id(pid) {

        //other sniffed packets
        color_presets["static_purple"] = {0x05, 0x0f, 0x01, 0x10, 0x03, 0x08, 0xbf, 0x00, 0xf9, 0x01, 0x00, 0x01, 0x00};
        color_presets["static_blue"]   = {0x05, 0x0f, 0x01, 0x10, 0x03, 0x08, 0x00, 0x00, 0xf9, 0x01, 0x00, 0x00, 0x00};
        //                               {0x05, 0x0f, 0x01, 0x10, 0x03, 0x10, 0x00, 0x90, 0x90, 0x01, 0x00, 0x01, 0x00};
    }

    ~MouseController() {
        disconnect();
    }

    //scans for target interface and gets its path then opens a connection to it
    bool connect(int target_interface = 2) {
        if (hid_init() < 0) {
            std::cerr << "Failed to initialize HIDAPI." << std::endl;
            return false;
        }

        struct hid_device_info* devs = hid_enumerate(vendor_id, product_id);
        struct hid_device_info* cur_dev = devs;
        const char* target_path = nullptr;

        while (cur_dev) {
            if (cur_dev->interface_number == target_interface) {
                target_path = cur_dev->path;
                break;
            }
            cur_dev = cur_dev->next;
        }

        if (target_path) {
            handle = hid_open_path(target_path);
        }

        hid_free_enumeration(devs);
        return handle != nullptr;
    }

    //closes connection
    void disconnect() {
        if (handle) {
            hid_close(handle);
            handle = nullptr;
            hid_exit();
        }
    }

    //dyanamically calculates checksum
    uint8_t calculate_checksum(const std::vector<uint8_t>& packet) {
        uint8_t sum = 0;
        size_t limit = (packet.size() >= 11) ? 11 : packet.size();

        for (size_t i = 0; i < limit; i++) {
            sum += packet[i];
        }
        return sum - 0x15;
    }


    //sends a two stage feature report sequence to the mouse.
    bool send_color_packet(std::vector<uint8_t> payload) {
        if (!handle) return false;

        std::vector<uint8_t> stage_header = {0x0c, 0x0a, 0x01, 0xfe, 0x01, 0xfe}; //get ready to accept info

        // Calculate and attach dynamic checksum
        payload[payload.size() - 1] = calculate_checksum(payload);

        int res1 = hid_send_feature_report(handle, stage_header.data(), stage_header.size());
        usleep(50000);
        int res2 = hid_send_feature_report(handle, payload.data(), payload.size());

        return (res1 >= 0 && res2 >= 0);
    }


    // saves current settings to a text file (Meets File I/O requirement).
    void save_profile(const std::string& filepath, const std::string& profile_name) {
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << "ACTIVE_PROFILE=" << profile_name << "\n";
            file.close();
            std::cout << "Saved profile selection to " << filepath << std::endl;
        }
    }
};

int main() {
    MouseController mouse(MOUSE_VID, MOUSE_PID);

    //call mouse connect and pass the target interface
    if (!mouse.connect(2)) {
        std::cerr << "Could not open target mouse interface." << std::endl;
        return -1;
    }

    std::cout << "Connected to mouse successfully!" << std::endl;

    // Send profile and save choice
    std::vector<uint8_t> test_packet = {0x05, 0x0F, 0x01, 0x30, 0x03, 0x08, 0xEA, 0x00, 0x94, 0x01, 0x00, 0x01, 0xAA};
    mouse.send_color_packet(test_packet);
    mouse.save_profile("mouse_config.txt", "this_ones");

    return 0;
}
