#pragma once

class Transform {
public:
    void setFromMatrix(const glm::mat4& matrix);

    void setPosition(const glm::vec3& pos);
    void setRotation(const glm::quat& rot);
    void setRotationEuler(const glm::vec3& eulerDegrees);
    void setScale(const glm::vec3& scale);

    void rotate(const glm::quat& rot);
    void rotate(const glm::vec3& eulerDegrees);

    const glm::vec3& getPosition() const { return m_position; }
    const glm::quat& getRotation() const { return m_rotation; }
    const glm::vec3& getScale() const { return m_scale; }
    glm::vec3 getEulerAngles() const;

    const glm::mat4& getLocalMatrix() const;

    bool isDirty() const { return m_dirty; }
    void setDirty() { m_dirty = true; }

private:
    glm::vec3 m_position{0.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_scale{1.0f};

    mutable glm::mat4 m_localMatrix{1.0f};
    mutable bool m_dirty = true;
};
