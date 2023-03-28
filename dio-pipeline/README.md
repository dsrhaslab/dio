# DIO analysis pipeline

Ansible playbook to install and run the DIO analysis pipeline.

## Install ansible and required modules

```
apt install ansible
ansible-galaxy collection install ansible.posix
ansible-galaxy collection install kubernetes.core
ansible-galaxy collection install cloud.common
ansible-galaxy collection install community.general
ansible-galaxy collection install community.kubernetes
```

## Install docker and kubernetes

### Edit inventory file (hosts.ini)

1. Add the master information in the group "[master]" (syntax below)
2. Add the workers information in the group "[node]" (syntax below)

Syntax:
```
<hostname> ansible_host=<host_ip> ansible_python_interpreter='python3'
```

### Install docker and kubernetes

#### On remote hosts:

```
ansible-playbook -u <username> -i hosts.ini playbook.yml
```

#### On vms:

```
ansible-playbook -u <username> -i hosts.ini playbook.yml -e is_vm=true
```

## Install DIO pipeline

### Prepare setup for DIO pipeline:
```
ansible-playbook -u <username> -i hosts.ini dio_playbook.yml --tags prepare_setup
```

### Deploy DIO pipeline:

#### From scratch:
```
ansible-playbook -u <username> -i hosts.ini dio_playbook.yml --tags deploy_dio -e run_all=true
```

#### From previous configuration:
```
ansible-playbook -u <username> -i hosts.ini dio_playbook.yml --tags deploy_dio
```

### Delete DIO pipeline

#### Full delete:
```
ansible-playbook -u <username> -i hosts.ini dio_playbook.yml --tags delete_dio -e run_all=true
```

#### Keep PVs:
```
ansible-playbook -u <username> -i hosts.ini dio_playbook.yml --tags delete_dio
```

---

Extended from: https://github.com/kairen/kubeadm-ansible.git
