/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "libvirt_virtual_machine_factory.h"
#include "libvirt_virtual_machine.h"

#include <multipass/logging/log.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <shared/linux/backend_utils.h>

#include <multipass/format.h>

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto multipass_bridge_name = "mpvirtbr0";
constexpr auto logging_category = "libvirt-factory";

auto generate_libvirt_bridge_xml_config(const mp::Path& data_dir, const std::string& bridge_name)
{
    auto network_dir = mp::utils::make_dir(QDir(data_dir), "network");
    auto subnet = mp::backend::get_subnet(network_dir, QString::fromStdString(bridge_name));

    return fmt::format("<network>\n"
                       "  <name>default</name>\n"
                       "  <bridge name=\"{}\"/>\n"
                       "  <domain name=\"multipass\" localOnly=\"yes\"/>\n"
                       "  <forward/>\n"
                       "  <ip address=\"{}.1\" netmask=\"255.255.255.0\">\n"
                       "    <dhcp>\n"
                       "      <range start=\"{}.2\" end=\"{}.254\"/>\n"
                       "    </dhcp>\n"
                       "  </ip>\n"
                       "</network>",
                       bridge_name, subnet, subnet, subnet);
}

std::string enable_libvirt_network(const mp::Path& data_dir)
{
    mp::LibVirtVirtualMachine::ConnectionUPtr connection{nullptr, nullptr};
    try
    {
        connection = mp::LibVirtVirtualMachine::open_libvirt_connection();
    }
    catch (const std::exception&)
    {
        return {};
    }

    mp::LibVirtVirtualMachine::NetworkUPtr network{virNetworkLookupByName(connection.get(), "default"), virNetworkFree};
    std::string bridge_name;

    if (network == nullptr)
    {
        bridge_name = multipass_bridge_name;
        network = mp::LibVirtVirtualMachine::NetworkUPtr{
            virNetworkCreateXML(connection.get(), generate_libvirt_bridge_xml_config(data_dir, bridge_name).c_str()),
            virNetworkFree};
    }
    else
    {
        auto libvirt_bridge = virNetworkGetBridgeName(network.get());
        bridge_name = libvirt_bridge;
        free(libvirt_bridge);
    }

    if (virNetworkIsActive(network.get()) == 0)
    {
        virNetworkCreate(network.get());
    }

    return bridge_name;
}
} // namespace

mp::LibVirtVirtualMachineFactory::LibVirtVirtualMachineFactory(const mp::Path& data_dir)
    : data_dir{data_dir}, bridge_name{enable_libvirt_network(data_dir)}
{
}

mp::VirtualMachine::UPtr mp::LibVirtVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                                  VMStatusMonitor& monitor)
{
    if (bridge_name.empty())
        bridge_name = enable_libvirt_network(data_dir);

    return std::make_unique<mp::LibVirtVirtualMachine>(desc, bridge_name, monitor);
}

mp::LibVirtVirtualMachineFactory::~LibVirtVirtualMachineFactory()
{
    if (bridge_name == multipass_bridge_name)
    {
        auto connection = LibVirtVirtualMachine::open_libvirt_connection();
        mp::LibVirtVirtualMachine::NetworkUPtr network{virNetworkLookupByName(connection.get(), "default"),
                                                       virNetworkFree};

        virNetworkDestroy(network.get());
    }
}

void mp::LibVirtVirtualMachineFactory::remove_resources_for(const std::string& name)
{
    auto connection = LibVirtVirtualMachine::open_libvirt_connection();

    virDomainUndefine(virDomainLookupByName(connection.get(), name.c_str()));
}

mp::FetchType mp::LibVirtVirtualMachineFactory::fetch_type()
{
    return mp::FetchType::ImageOnly;
}

mp::VMImage mp::LibVirtVirtualMachineFactory::prepare_source_image(const VMImage& source_image)
{
    VMImage image{source_image};
    image.image_path = mp::backend::convert_to_qcow_if_necessary(source_image.image_path);
    return image;
}

void mp::LibVirtVirtualMachineFactory::prepare_instance_image(const VMImage& instance_image,
                                                              const VirtualMachineDescription& desc)
{
    mp::backend::resize_instance_image(desc.disk_space, instance_image.image_path);
}

void mp::LibVirtVirtualMachineFactory::configure(const std::string& /*name*/, YAML::Node& /*meta_config*/,
                                                 YAML::Node& /*user_config*/)
{
}

void mp::LibVirtVirtualMachineFactory::hypervisor_health_check()
{
    mp::backend::check_for_kvm_support();
    mp::backend::check_if_kvm_is_in_use();

    auto connection = LibVirtVirtualMachine::open_libvirt_connection();

    if (bridge_name.empty())
        bridge_name = enable_libvirt_network(data_dir);
}

QString mp::LibVirtVirtualMachineFactory::get_backend_version_string()
{
    try
    {
        unsigned long libvirt_version;
        auto connection = LibVirtVirtualMachine::open_libvirt_connection();

        if (virConnectGetVersion(connection.get(), &libvirt_version) == 0 && libvirt_version != 0)
        {
            return QString("libvirt-%1.%2.%3")
                .arg(libvirt_version / 1000000)
                .arg(libvirt_version / 1000 % 1000)
                .arg(libvirt_version % 1000);
        }
    }
    catch (const std::exception&)
    {
        // Ignore
    }

    mpl::log(mpl::Level::error, logging_category, "Failed to determine libvirtd version.");
    return QString("libvirt-unknown");
}
