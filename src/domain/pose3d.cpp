#include "knowledge_manager/domain/pose3d.hpp"

namespace knowledge_manager::domain
{

Pose3D::Pose3D()
: x_(0.0f), y_(0.0f), z_(0.0f), qx_(0.0f), qy_(0.0f), qz_(0.0f), qw_(1.0f)
{
}

Pose3D::Pose3D(
  float x, float y, float z, float qx, float qy, float qz,
  float qw)
: x_(x), y_(y), z_(z), qx_(qx), qy_(qy), qz_(qz), qw_(qw) {}

float Pose3D::getX() const {return x_;}
float Pose3D::getY() const {return y_;}
float Pose3D::getZ() const {return z_;}
float Pose3D::getQx() const {return qx_;}
float Pose3D::getQy() const {return qy_;}
float Pose3D::getQz() const {return qz_;}
float Pose3D::getQw() const {return qw_;}

void Pose3D::setPosition(float x, float y, float z)
{
  x_ = x;
  y_ = y;
  z_ = z;
}

void Pose3D::setOrientation(float qx, float qy, float qz, float qw)
{
  qx_ = qx;
  qy_ = qy;
  qz_ = qz;
  qw_ = qw;
}

} // namespace knowledge_manager::domain
