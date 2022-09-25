#include "Renderer.h"
#include "Walnut/Random.h"

namespace Utils {
	static uint32_t ConvertToRGBA(const glm::vec4& color)
	{
		uint8_t r = (uint8_t)(color.r * 255.0f);
		uint8_t g = (uint8_t)(color.g * 255.0f);
		uint8_t b = (uint8_t)(color.b * 255.0f);
		uint8_t a = (uint8_t)(color.a * 255.0f);

		uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;
		return result;
	}
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_FinalImage)
	{
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
			return;

		m_FinalImage->Resize(width, height);
	}
	else
	{
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}

	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	Ray ray;
	ray.Origin = camera.GetPosition();

	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			ray.Direction = camera.GetRayDirections()[x + y * m_FinalImage->GetWidth()];

			glm::vec4 color = TraceRay(scene, ray);
			color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(color);
		}
	}

	m_FinalImage->SetData(m_ImageData);
}

glm::vec4 Renderer::TraceRay(const Scene& scene, const Ray& ray)
{
	glm::vec3 lightDirection = glm::normalize(glm::vec3(-1, -1, -1));
	//rayDirection = glm::normalize(rayDirection);

	// (bx^2 + by^2 + bz^2)t^2 + (2(axbx + ayby + azbz)t + (ax^2 + ay^2 - r^2) = 0
	// where
	// a - ray origin
	// b - ray direction
	// r - sphere radius
	// t - hit dist

	if (scene.Spheres.size() == 0)
	{
		return glm::vec4(0, 0, 0, 1);
	}

	const Sphere* closestSphere = nullptr;
	float hitDist = FLT_MAX;
	for (const Sphere& sphere : scene.Spheres)
	{
		glm::vec3 origin = ray.Origin - sphere.Position;

		float aTerm = glm::dot(ray.Direction, ray.Direction);
		float bTerm = 2.0f * glm::dot(origin, ray.Direction);
		float cTerm = glm::dot(origin, origin) - sphere.Radius * sphere.Radius;

		//D = b^2 - 4ac
		float D = bTerm * bTerm - 4.0f * aTerm * cTerm;
		if (D <= 0.0f)
			continue;

		// x12 = (-b +- sqrt(D)) / 2a
		float closestRoot = (-bTerm - glm::sqrt(D)) / (2.0f * aTerm); //closest root
		if (closestRoot < hitDist)
		{
			hitDist = closestRoot;
			closestSphere = &sphere;
		}
	}

	if (closestSphere == nullptr)
	{
		return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	glm::vec3 origin = ray.Origin - closestSphere->Position;

	//glm::vec3 hit1 = rayOrigin + rayDirection * root0;
	glm::vec3 hit2 = origin + ray.Direction * hitDist;
	glm::vec3 normal = glm::normalize(hit2);
	glm::vec3 sphereColor = closestSphere->Albedo;

	float angle = glm::max(glm::dot(normal, -lightDirection), 0.0f);

	sphereColor *= angle;
	return glm::vec4(sphereColor, 1.0f);
}
