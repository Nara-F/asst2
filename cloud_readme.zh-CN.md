# AWS 配置说明

性能测试需要在 AWS 虚拟机上运行。使用课程提供的学生优惠券计费。每天结束后请关闭实例，避免夜间消耗额度。

## 创建虚拟机

1. 登录 AWS EC2 控制台，选择 us-west-2 区域并点击 Launch instance。
2. 选择 Ubuntu Server 24.04 LTS AMI。
3. 选择 c7g.4xlarge。开发阶段建议尽量使用本地或 myth，只在性能调优时使用 AWS；少量实验可使用更便宜的 c7g.xlarge。
4. 将磁盘调整为 64 GB。
5. 创建 key pair，下载 key 文件，然后启动实例。
6. 在实例 Connect 页面找到 public IPv4 DNS。先执行 chmod 400 path/to/key_name.pem，再连接：

    ssh -i path/to/key_name.pem ubuntu@<public_dns_name>

c7g.4xlarge 每小时费用较高，不使用时务必停止。SSH 登录后可安装 make、g++、python3 和 vim。建议使用私有 Git 仓库保存代码；必要时可用 scp 取回文件。停止实例可在控制台选择 Stop instance，或执行 sudo shutdown -P now。终止实例会删除实例及其文件。
