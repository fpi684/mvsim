# ROS2 launch file

from launch import LaunchDescription
from launch.substitutions import TextSubstitution
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from ament_index_python import get_package_share_directory
import os


def generate_launch_description():
    mvsimDir = get_package_share_directory("mvsim")
    # print('mvsimDir: ' + mvsimDir)

    # args that can be set from the command line or a default will be used
    world_file_launch_arg = DeclareLaunchArgument(
        "world_file", default_value=TextSubstitution(
            text=os.path.join(mvsimDir, 'mvsim_tutorial', 'demo_warehouse.world.xml')))

    headless_launch_arg = DeclareLaunchArgument(
        "headless", default_value='False')

    do_fake_localization_arg = DeclareLaunchArgument(
        "do_fake_localization", default_value='True', description='publish tf odom -> base_link')

    force_publish_vehicle_namespace_arg = DeclareLaunchArgument(
        "force_publish_vehicle_namespace", default_value='False',
        description='Use vehicle name namespace even if there is only one vehicle')

    mvsim_node = Node(
        package='mvsim',
        executable='mvsim_node',
        name='mvsim',
        output='screen',
        parameters=[
            os.path.join(mvsimDir, 'mvsim_tutorial',
                         'mvsim_ros2_params.yaml'),
            {
                "world_file": LaunchConfiguration('world_file'),
                "headless": LaunchConfiguration('headless'),
                "do_fake_localization": LaunchConfiguration('do_fake_localization'),
                "force_publish_vehicle_namespace": LaunchConfiguration('force_publish_vehicle_namespace'),
            }]
    )

    rviz2_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        arguments=[
                '-d', [os.path.join(mvsimDir, 'mvsim_tutorial', 'demo_warehouse_ros2.rviz')]]
    )

    return LaunchDescription([
        world_file_launch_arg,
        headless_launch_arg,
        do_fake_localization_arg,
        force_publish_vehicle_namespace_arg,
        mvsim_node,
        rviz2_node
    ])
