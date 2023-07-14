#include "Renderer.h"
#include "Walnut/Random.h"

#include <execution>

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

void Renderer::OnResize(uint32_t width, uint32_t height) {
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

	delete[] m_AccData;
	m_AccData = new glm::vec4[width * height];

	m_ImageHorizontalIter.resize(width);
	m_ImageVerticalIter.resize(height);

	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIter[i] = i;

	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIter[i] = i;
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;

	if (m_FrameIndex == 1) {
		memset(m_AccData, 0, m_FinalImage->GetHeight() * m_FinalImage->GetWidth() * sizeof(glm::vec4));
	}

#define MT 1
#if MT
	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),
		[this](uint32_t y) {
			std::for_each(std::execution::par, m_ImageHorizontalIter.begin(), m_ImageHorizontalIter.end(),
				[this, y](uint32_t x) {
					glm::vec4 color = PerPixel(x, y);
					m_AccData[x + y * m_FinalImage->GetWidth()] += color;

					glm::vec4 AccColor = m_AccData[x + y * m_FinalImage->GetWidth()];
					AccColor /= (float)m_FrameIndex;

					AccColor = glm::clamp(AccColor, glm::vec4(0.0f), glm::vec4(1.0f));
					m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(AccColor);
				});
		});
#else
	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			glm::vec4 color = PerPixel(x, y);
			m_AccData[x + y * m_FinalImage->GetWidth()] += color;

			glm::vec4 AccColor = m_AccData[x + y * m_FinalImage->GetWidth()];
			AccColor /= (float)m_FrameIndex;

			AccColor = glm::clamp(AccColor, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(AccColor);
		}
	}
#endif
	m_FinalImage->SetData(m_ImageData);

	if (m_Settings.Accumulate) {
		m_FrameIndex++;
	}
	else {
		m_FrameIndex = 1;
	}
}

Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{
	//rayDirection = glm::normalize(rayDirection);

	// (bx^2 + by^2 + bz^2)t^2 + (2(axbx + ayby + azbz)t + (ax^2 + ay^2 - r^2) = 0
	// where
	// a - ray origin
	// b - ray direction
	// r - sphere radius
	// t - hit dist

	int closestSphere = -1;
	float hitDist = std::numeric_limits<float>::max();
	for (size_t i = 0; i < m_ActiveScene->Spheres.size(); i++)
	{
		const Sphere& sphere = m_ActiveScene->Spheres[i];
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
		if (closestRoot > 0.0f && closestRoot < hitDist)
		{
			hitDist = closestRoot;
			closestSphere = (int)i;
		}
	}

	if (closestSphere < 0)
	{
		return Miss(ray);
	}

	return ClosestHit(ray, hitDist, closestSphere);
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y)
{
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];

	glm::vec3 light(0.0f);
	glm::vec3 absorption(1.0f);


	int bounces = 5;
	for (int i = 0; i < bounces; i++)
	{
		Renderer::HitPayload payload = TraceRay(ray);
		if (payload.HitDistance < 0.0f)
		{
			glm::vec3 skyColor = glm::vec3(0.6f, 0.7f, 0.9f);
			//light += skyColor * absorption;
			break;
		}

		const Sphere& sphere = m_ActiveScene->Spheres[payload.ObjectIndex];
		const Material& material = m_ActiveScene->Materials[sphere.MaterialIndex];

		light += material.getEmission();
		absorption *= material.Albedo;

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		/*ray.Direction = glm::reflect(ray.Direction,
			payload.WorldNormal + material.Roughness * Walnut::Random::Vec3(-0.5f, 0.5f));*/
		ray.Direction = glm::normalize(payload.WorldNormal + Walnut::Random::InUnitSphere());
	}

	return glm::vec4(light, 1.0f);
}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float hitdistance, int objectIndex)
{
	Renderer::HitPayload payload;
	payload.HitDistance = hitdistance;
	payload.ObjectIndex = objectIndex;

	const Sphere& closestSphere = m_ActiveScene->Spheres[objectIndex];
	glm::vec3 origin = ray.Origin - closestSphere.Position;
	payload.WorldPosition = origin + ray.Direction * hitdistance;
	payload.WorldNormal = glm::normalize(payload.WorldPosition);

	payload.WorldPosition += closestSphere.Position;

	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayload payload;
	payload.HitDistance = -1;
	return payload;
}
