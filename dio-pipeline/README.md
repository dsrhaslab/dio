# DIO analysis pipeline (via docker-compose)

Docker-compose for installing and running DIO's analysis pipeline (local setup).

### Install dependencies
- docker (v24.0.5)
- docker-compose (v1.29.2)

### Setup environment

The folder [docker-compose](docker-compose) contains a _docker-compose.yml_ file that configures one docker container for Elasticsearch and another for Kibana, and an _.env_ file that contains important variables for setting up DIO's analysis pipeline.

1. Update the necessary variables in the _.env_ file according to your setup.
2. Run `docker-compose up`
3. Ensure that you can access and login into kibana:
    - Access [http://<HOST_IP>:<KIBANA_PORT>]()
    - Login in with:
        - Username: _elastic_
        - Password: the elastic password defined in .env (e.g., _secret_).
4. Import DIO's dashboards into Kibana:
    ```
    curl -u "elastic:<ELASTIC_PASSWORD>" -X POST -k "http://<HOST_IP>:<KIBANA_PORT>/api/saved_objects/_import" -H "kbn-xsrf: true" --form file=@dio_dashboards.ndjson
    ```

# DIO analysis pipeline (via Kubernetes)

Ansible playbook for installing and running DIO's analysis pipeline (local or distributed setup).

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
