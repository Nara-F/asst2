# AWS 配置说明

本作业的性能测试需要在 Amazon Web Services（AWS）虚拟机实例上运行。我们将提供（或已经发放）可用于付费的学生代金券。下面介绍配置步骤。

> [!NOTE]
> 每天结束后请关闭实例，避免一夜之间耗尽额度！

### 创建虚拟机

1. 登录 `us-west-2` 区域的 [AWS EC2 控制台](https://us-west-2.console.aws.amazon.com/ec2/home?region=us-west-2#Home)，点击 `Launch instance`。
![启动实例](figs/launch_instance.png?raw=true)

> [!TIP]
> 建议在本地或 myth 上完成大部分开发，只用 AWS 机器进行性能调优。

2. 选择 `Ubuntu Server 24.04 LTS (HVM), SSD Volume Type` AMI。
![AMI](https://github.com/user-attachments/assets/34501c4a-a0d8-47d9-a622-f0dc6ffdaa4e)

3. 选择 `c7g.4xlarge` 实例类型。
![实例类型](https://github.com/user-attachments/assets/ecbe5840-4751-4dc8-b348-0620005679e7)

> [!TIP]
> 如果在本地或 myth 上完成大部分开发后，仍需在 AWS 上做实验，可以使用有 4 个 CPU 核心、价格更低的 `c7g.xlarge`。

> [!IMPORTANT]
> 按原文所列费用，`c7g.4xlarge` 每小时 0.58 美元，运行一整天会消耗 13.92 美元代金券。不用时请关闭实例！

4. 将卷大小改为 64 GB，以便安装作业所需的软件包。
![存储](https://github.com/user-attachments/assets/4d736f35-a0bd-4e59-8b3e-329289105a11)

5. 访问实例需要密钥对。在 `Key pair (login)` 中点击 `Create a new key pair`，自行命名。浏览器会下载 `<key_name>.pem` 密钥文件，用它登录即将创建的虚拟机。之后即可启动实例。
![创建密钥对，第 1 步](figs/keypair_step1.png)
![创建密钥对，第 2 步](https://github.com/user-attachments/assets/6fd9f6f7-668a-4842-b196-30176c962e5a)

6. 确认所有信息并启动实例。
![启动实例](https://github.com/user-attachments/assets/7e6deb82-65fb-4ed9-a372-f56a2a437506)

7. 创建虚拟机后即可通过 **SSH** 登录。进入实例页面，点击 `Connect`，选择 SSH 标签页，查找公有 IPv4 DNS 名称。实例启动和分配 IP 可能需要一些时间。
![连接](figs/connect.png?raw=true)

按提示执行 `chmod 400 path/to/key_name.pem` 修改密钥文件权限。取得地址后，执行以下命令登录：

~~~~
ssh -i path/to/key_name.pem ubuntu@<public_dns_name>
~~~~

> [!WARNING]
> 创建实例后，如果配置中途需要离开，请先关闭实例。持续运行可能耗尽额度并产生额外费用。

### 配置虚拟机环境

8. SSH 登录后，安装开发所需的软件。例如建议安装：

~~~~
sudo apt update
sudo apt install make g++ python3 # Required
sudo apt install vim
~~~~

9. 将[起始代码](https://github.com/stanford-cs149/asst2/archive/refs/heads/master.zip)下载到本地，[创建 Git 仓库](https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository)（也可使用其他版本控制系统），**务必保持仓库私有**，再在虚拟机上克隆该仓库。

### 从 AWS 取回代码

需要时可以用以下命令取回虚拟机上的文件：

~~~~
scp -i path/to/key_name.pem ubuntu@<public_dns_name>:/path/to/file /path/to/local_file
~~~~

强烈建议使用版本控制，并通过本地 VS Code 访问代码；不建议依靠 `scp` 来回传输代码。

### 停止虚拟机

10. 进入实例页面，点击 `Instance state`，选择 `Stop instance` 停止实例。
![停止实例](figs/stop_instance.png?raw=true)

注意，终止（terminate）实例会删除实例及其关联文件。停止（stop）实例可在 AWS 网页控制台操作，也可登录后执行 `sudo shutdown -P now`。

如果对步骤有疑问、配置账号时遇到问题，或有其他问题，请在 Ed 联系课程团队。

> [!CAUTION]
> 最后再提醒一次：**不用时务必关闭实例。**
>
> 无论出于何种原因超出分配额度，我们都可能无法补充 AWS 额度或报销费用。
