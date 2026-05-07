# DPDK Install
https://doc.dpdk.org/guides/linux_gsg/index.html

## Install
```bash
sudo apt install build-essential


sudo apt update
sudo apt install dpdk dpdk-dev -y

```
## Post Installation  
### Setup hugepages
```bash
echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
modprobe vfio_iommu_type1 dma_entry_limit=512000
sudo chmod a+w -R /dev/hugepages

```

### Load VIFO driver
```bash
sudo modprobe vfio-pci
```
### Bind Network Port (Example):
Identify your interface and bind it using the provided tool.bash# List devices
```bash
dpdk-devbind.py -s 
# Bind to vfio-pci
sudo dpdk-devbind.py -b vfio-pci 0000:72:00.0
```
## Verify
```bash
dpdk-testpmd -l 1-2 -n 4 -- -i
```

# TREX
https://blog.hacksbrain.com/cisco-trex-packet-generator-step-by-step

## Install
```bash
 sudo mkdir -p /opt/trex
 cd /opt/trex
 sudo wget -no-check-certificate --no-cache https://trex-tgn.cisco.com/trex/release/latest
 sudo tar -xzvf latest
 sudo rm latest
```
## Verify
```bash
cd /opt/trex/v3.08/
sudo ./dpdk_setup_ports.py -t
```
