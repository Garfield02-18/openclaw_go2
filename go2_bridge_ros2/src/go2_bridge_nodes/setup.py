from setuptools import setup

package_name = 'go2_bridge_nodes'

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='radxa',
    maintainer_email='radxa@local',
    description='Standalone ROS2 bridge nodes for OpenClaw to Go2 routing.',
    license='Apache-2.0',
    entry_points={
        'console_scripts': [
            'go2_bridge_node = go2_bridge_nodes.go2_bridge_node:main',
            'go2_command_client = go2_bridge_nodes.go2_command_client:main',
        ],
    },
)
