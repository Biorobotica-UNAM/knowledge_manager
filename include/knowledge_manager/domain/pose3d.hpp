#pragma once

namespace knowledge_manager::domain
{

class Pose3D {
public:
  Pose3D();
  Pose3D(float x, float y, float z, float qx, float qy, float qz, float qw);

  float getX() const;
  float getY() const;
  float getZ() const;
  float getQx() const;
  float getQy() const;
  float getQz() const;
  float getQw() const;

  void setPosition(float x, float y, float z);
  void setOrientation(float qx, float qy, float qz, float qw);

private:
  float x_;
  float y_;
  float z_;
  float qx_;
  float qy_;
  float qz_;
  float qw_;
};

} // namespace knowledge_manager::domain
