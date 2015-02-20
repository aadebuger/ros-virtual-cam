/**
 * Based on https://github.com/czw90130/virtual_camera/ by:
 *
 * @copyright Copyright (c) 2013, Zhiwei Chu
 * @copyright Copyright (c) 2015, mayfieldrobotics.
 * @license This project is released under the BSD License.
 *
 */

#include "command_line.h"
#include "image_converter.h"
#include "video_device.h"

#include <image_transport/image_transport.h>
#include <image_transport/publisher.h>
#include <image_transport/subscriber.h>
#include <ros/ros.h>
#include <sensor_msgs/Image.h>

class ImageStream {

public:

    ImageStream(const std::string& topic, ImageConverter& cvt, VideoDevice& dev, size_t queue_size) :
        _dev(dev),
        _cvt(cvt),
        _transport(ros::NodeHandle()),
        _sub(_transport.subscribe(topic, queue_size, &ImageStream::stream, this)) {
    }

    void stream(const sensor_msgs::ImageConstPtr& msg) {
        int rc = _cvt(msg, _buf);
        if (rc == -1) {
            return;
        }
        rc = _dev.write(&_buf[0], _buf.size());
        if (rc == -1) {
            return;
        }
    }

    void operator()(const sensor_msgs::ImageConstPtr& msg) {
        stream(msg);
    }

private:

    ImageConverter _cvt;

    ImageConverter::Buffer _buf;

    VideoDevice _dev;

    image_transport::ImageTransport _transport;

    image_transport::Subscriber _sub;

};

const int EXIT_OK = 0;
const int EXIT_FAILED_DEVICE_FORMAT = 1;
const int EXIT_FAILED_DEVICE_STREAM = 2;
const int EXIT_FAILED_DEVICE_CAPS = 3;


int main(int argc, char**argv) {
    ros::V_string args;
    ros::removeROSArgs(argc, argv, args);
    CommandLine cl;
    cl(args);

    ros::init(argc, argv, "stream", ros::init_options::AnonymousName);

    ROS_INFO(
        "converting - width=%zu, height=%zu, fourcc=%s",
        cl.video_size().width, cl.video_size().height, cl.video_fourcc().c_str()
    );
    ImageConverter cvt(
        cl.video_size().width,
        cl.video_size().height,
        cl.video_fourcc()
    );

    ROS_INFO("opening '%s'", cl.video_device().c_str());
    VideoDevice dev(cl.video_device());

    ROS_INFO("setting '%s' format", cl.video_device().c_str());
    v4l2_format format = cvt.format();
    int rc = dev.set_format(format);
    if (rc == -1) {
        return EXIT_FAILED_DEVICE_FORMAT;
    }

    ROS_INFO("turn on '%s' streaming", cl.video_device().c_str());
    rc = dev.stream_on();
    if (rc == -1) {
        return EXIT_FAILED_DEVICE_STREAM;
    }

    v4l2_capability capability;
    rc = dev.capabilities(capability);
    if (rc == -1) {
        return EXIT_FAILED_DEVICE_CAPS;
    }
    ROS_INFO("'%s' caps %#08x", cl.video_device().c_str(), capability.capabilities);

    ROS_INFO(
        "streaming images from '%s' to '%s' w/ queue-size=%zu",
        ros::names::resolve("image", true).c_str(),
        cl.video_device().c_str(),
        cl.queue_size()
    );
    ImageStream stream("image", cvt, dev, cl.queue_size());

    ros::spin();

    return EXIT_OK;
}