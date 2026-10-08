#include"SceneObject.h"
#include"Model.h"

SceneObject::SceneObject(const std::string& name, Model* model) :name(name), model(model)
{

}


bool SceneObject::getWorldBounds(AABB& outBounds) const
{
	if (model == nullptr)
	{
        if (light.has_value())
        {
            const float editorSize = 0.5f;

            glm::vec3 extent(editorSize);

            outBounds.min = transform.position - extent;

            outBounds.max = transform.position + extent;

            return true;
        }

		return false;
	}

	glm::vec3 localMin = model->getBoundsMin();
	glm::vec3 localMax = model->getBoundsMax();

    glm::vec3 corners[8] =
    {
        { localMin.x, localMin.y, localMin.z },
        { localMax.x, localMin.y, localMin.z },
        { localMin.x, localMax.y, localMin.z },
        { localMax.x, localMax.y, localMin.z },

        { localMin.x, localMin.y, localMax.z },
        { localMax.x, localMin.y, localMax.z },
        { localMin.x, localMax.y, localMax.z },
        { localMax.x, localMax.y, localMax.z }
    };

    glm::mat4 modelMatrix = transform.getMatrix();
    glm::vec3 firstCorner = glm::vec3(modelMatrix * glm::vec4(corners[0], 1.0f));

    outBounds.min = firstCorner;
    outBounds.max = firstCorner;

    for (int i = 1; i < 8; ++i)
    {
        glm::vec3 worldCorner = glm::vec3(modelMatrix * glm::vec4(corners[i], 1.0));
        
        outBounds.min = glm::min(outBounds.min, worldCorner);

        outBounds.max = glm::max(outBounds.max, worldCorner);
    }

    return true;
}