# DIO
## A tool for diagnosing applications I/O behavior through system call observability

DIO is a generic tool for observing and diagnosing applications storage I/O.

It is designed to be used by applications developers and users to understand how applications interact with storage systems.

By combining system call tracing with a customizable data analysis and visualization pipeline, DIO provide non-intrusive and comprehensive I/O diagnosis for applications using in-kernel POSIX storage systems (e.g., ext4, linux block device).

DIO's website: https://dio-tool.netlify.app

## Features

* **Generic and non-intrusive**: DIO can be used to observe I/O system calls made by any application interacting with in-kernel storage systems without requiring any modification to the application code.

* **Flexible**: DIO can be configured to collect only the information that is relevant to the user. Namely, DIO allows users to filter events based on:
    * process name (command)
    * process/thread IDs
    * system call types
    * file paths

* **Comprehensive**: DIO collects a wide range of information about the I/O system calls made by applications. Namely, DIO collects:
    * system call type, arguments and return value
    * process name (command), process ID, thread ID
    * start and end timestamps
    * additional context from the kernel:
        * file type
        * file offset
        * file tag

* **Pratical and timely analysis**: DIO provides a full pipeline to capture, analyze and visualize collected data in a timely manner. Namely, DIO provides:
    * a *tracer* component that intercepts system calls made by applications.
    * a *backend* component that stores collected data in a database and enables users to query traced data, apply filters to analyze specific information, and correlate different types of data.
    * a *visualizer* component that allows users to query the database and visualize the results in a web interface and build customized visualizations.


## How it works

<p align="center">
  <img width="600" height="200" src="docs/images/DIO-design.svg">
</p>

### Tracer
The **tracer** component relies on the eBPF technology to intercept system calls done by applications in a non-intrusive way.

Briefly, it comprises a set of eBPF programs that, at the initialization phase (&#10122;), are attached to system calls tracepoints.

These eBPF programs will collect the relevant information about the system calls (in kernel) and place it in a ring buffer (&#10123;) to be accessed in user space.

At user space, the tracer is constantly pooling events from the ring buffer (&#10124;) and sending them to the backend (&#10125;) for storage.

### Backend
The **backend** component persists and indexes events (&#10126;), and allows users to query and summarize (e.g., aggregate) stored information (&#10127;).

It uses the Elasticsearch distributed engine for storing and processing large volumes of data.

By providing an interface for searching, querying, and updating data, the backend allows users to develop and integrate customized data correlation algorithms.

### Visualizer
The **visualizer** component provides near real-time visualization of the traced events by querying the backend (&#10128;).

It uses Kibana, the data visualization dashboard software for Elasticsearch, which offers a web interface for data exploration and analysis. Moreover, it allows users to select specific types of data (e.g., system call type, arguments) to build different and customized representations.


## Getting started with DIO

The installation and configuration of DIO are performed in two phases: *i)* the setup and initialization of the analysis pipeline and *ii)* the configuration and execution of the *tracer*.

### Set up DIO' pipeline

For setting up DIO's analysis pipeline, it is necessary to install Elasticsearch and Kibana software and upload DIO's pre-defined dashboards.
Folder `dio-pipeline` contains ansible-playbooks to automatically create and set up a Kubernetes cluster with all the required components.

1. After clonning the repo, move into `dio-pipeline` folder:
    ```
    git clone https://github.com/dsrhaslab/dio.git
    cd dio-pipeline
    ```

2. Install Ansible and the required modules:
    ```
    apt install ansible
    ansible-galaxy collection install ansible.posix
    ansible-galaxy collection install kubernetes.core
    ansible-galaxy collection install cloud.common
    ansible-galaxy collection install community.general
    ansible-galaxy collection install community.kubernetes
    ```

3. Update the inventory file (`hosts.ini`) with the information of the machines where the pipeline should be installed. If more than one machine is used, Kibana will be installed on the Master machine, while Elasticsearch will run on workers' machines.
    - Add the master information in the group "[master]"
    - Add the workers information in the group "[node]"

    Syntax:
    ```
    <hostname> ansible_host=<host_ip> ansible_python_interpreter='python3'
    ```

    <details>
        <summary>Example with 1 master and 2 workers</summary>

        [master]
        master ansible_host=192.168.56.100 ansible_python_interpreter='python3'

        [node]
        worker1 ansible_host=192.168.56.101 ansible_python_interpreter='python3'
        worker2 ansible_host=192.168.56.102 ansible_python_interpreter='python3'

        [kibana:children]
        master

        [kube_cluster:children]
        master
        node
    </details>

4. Run the `run_dio_pipeline.sh` script to install and configure DIO's pipeline:
    ```
    bash run_dio_pipeline.sh install_dio_pipeline
    ```

5. Access Kibana dashboards: http://<master/workers ip>:32222. Default credentials\*: are:
   	- Username: dio
   	- Password: diopw

    \*Credentials can be changed on the group_vars/kube_cluster.yml file.

More information at [dio-pipeline](dio-pipeline/README.md).

### Set up DIO' tracer

#### From source code

##### Dependencies
- GO (v.1.17.4)
- [BCC](https://github.com/iovisor/bcc.git) (#1313fd6a5)

##### Build dio-tracer binary
```
bash scripts/build.sh go
```

##### Run dio-tracer
```
sudo bin/dio-tracer [options] -- <TARGET_COMMAND>
```
<details>
    <summary>Example</summary>

    sudo bin/dio-tracer --config config.yaml -- ls
</details>

#### From Docker Image

##### Pull dio-tracer image
```
docker pull taniaesteves/dio:v1.0.1
```
##### Run dio-tracer
```
docker run -it --rm --name dio --pid=host --privileged --cap-add=ALL --net=host -v /lib/modules:/lib/modules -v /usr/src:/usr/src -v /sys/kernel/debug/:/sys/kernel/debug/ taniaesteves/dio:v1.0.1 <TARGET_COMMAND>
```

More information at [dio-tracer](dio-tracer/README.md).

###### Options:

- To change DIO's configuration file mount a volume for `/usr/share/dio/conf/config.yaml` (`-v <path_to_local_config_file:/usr/share/dio/conf/config.yaml`)
- To export DIO-tracer files (e.g., tracer logs and statistics), mount a volume for the `/dio_data` (`-v /tmp/dio_data:/dio_data`)
- To run DIO correlation path along with DIO's tracer use the following options:
    - `-e CORRELATE_PATHS=true`
    - `-e ES_SERVERS=<DIO_ES_URL>:<DIO_ES_PORT>`


## DIO use cases

#### Fluent Bit
Root cause analysis of data loss caused by erroneous file accesses. The full description, how to reproduce and the resulting visual representations can be consulted [here](https://dio-tool.netlify.app/use-cases/fluentbit).

#### RocksDB
Root cause analysis of resource contention in multi threaded I/O that leads to high tail latency for user workloads. The full description, how to reproduce and the resulting visual representations can be consulted [here](https://dio-tool.netlify.app/use-cases/rocksdb).

<!-- ## Additional information
**Publications about DIO.**
You can find [here](.docs/publications.md) a list of publications that extend, improve, and use DIO.

## Acknowledgments -->

## Contact

Please contact us at tania.c.araujo@inesctec.pt with any questions.