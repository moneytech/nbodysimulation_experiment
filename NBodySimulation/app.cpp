#include "app.h"

#ifndef APP_IMPLEMENTATION
#define APP_IMPLEMENTATION

#include <GL/glew.h>
#include <filesystem>
#include <iostream>
#include <fstream>

#include "sph.h"
#include "pseudorandom.h"
#include "chart.h"

#include "demo1.cpp"
#include "demo2.cpp"
#include "demo3.cpp"
#include "demo4.cpp"

Window::Window() :
	left(0),
	top(0),
	width(kWindowWidth),
	height(kWindowHeight) {
}

Application::Application() {
	window = new Window();
}

Application::~Application() {
	delete window;
}

void Application::Resize(const int width, const int height) {
	window->width = width;
	window->height = height;
}

DemoApplication::DemoApplication() :
	Application(),
	externalForcesApplying(false),
	multiThreadingActive(true),
	activeScenarioIndex(0),
	simulationActive(true),
	demoIndex(0),
	demo(nullptr) {

	demoStats.reserve(kDemoCount);
	LoadDemo(demoIndex);

	benchmarkActive = false;
	benchmarkDone = false;
	activeBenchmarkIteration = nullptr;
	benchmarkFrameCount = 0;
	benchmarkIterations.reserve(kBenchmarkIterationCount);

	//StartBenchmark();
}

DemoApplication::~DemoApplication() {
	delete demo;
}

void DemoApplication::PushDemoStatistics() {
	DemoStatistics demoStat = DemoStatistics();

	size_t avgCount = 0;
	demoStat.min.simulationTime = FLT_MAX;
	demoStat.max.simulationTime = 0.0f;
	demoStat.avg.simulationTime = 0.0f;

	size_t maxFrameCount = 0;
	const size_t iterationCount = benchmarkIterations.size();
	for (size_t iterationIndex = 0; iterationIndex < iterationCount; ++iterationIndex) {
		BenchmarkIteration *iteration = &benchmarkIterations[iterationIndex];

		size_t frameCount = iteration->frames.size();
		maxFrameCount = std::max(maxFrameCount, frameCount);

		for (size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
			FrameStatistics *frameStat = &iteration->frames[frameIndex];

			UpdateMin(demoStat.min.simulationTime, frameStat->simulationTime);
			UpdateMin(demoStat.min.stats.time.collisions, frameStat->stats.time.collisions);
			UpdateMin(demoStat.min.stats.time.deltaPositions, frameStat->stats.time.deltaPositions);
			UpdateMin(demoStat.min.stats.time.densityAndPressure, frameStat->stats.time.densityAndPressure);
			UpdateMin(demoStat.min.stats.time.emitters, frameStat->stats.time.emitters);
			UpdateMin(demoStat.min.stats.time.integration, frameStat->stats.time.integration);
			UpdateMin(demoStat.min.stats.time.neighborSearch, frameStat->stats.time.neighborSearch);
			UpdateMin(demoStat.min.stats.time.predict, frameStat->stats.time.predict);
			UpdateMin(demoStat.min.stats.time.updateGrid, frameStat->stats.time.updateGrid);
			UpdateMin(demoStat.min.stats.time.viscosityForces, frameStat->stats.time.viscosityForces);

			UpdateMax(demoStat.max.simulationTime, frameStat->simulationTime);
			UpdateMax(demoStat.max.stats.time.collisions, frameStat->stats.time.collisions);
			UpdateMax(demoStat.max.stats.time.deltaPositions, frameStat->stats.time.deltaPositions);
			UpdateMax(demoStat.max.stats.time.densityAndPressure, frameStat->stats.time.densityAndPressure);
			UpdateMax(demoStat.max.stats.time.emitters, frameStat->stats.time.emitters);
			UpdateMax(demoStat.max.stats.time.integration, frameStat->stats.time.integration);
			UpdateMax(demoStat.max.stats.time.neighborSearch, frameStat->stats.time.neighborSearch);
			UpdateMax(demoStat.max.stats.time.predict, frameStat->stats.time.predict);
			UpdateMax(demoStat.max.stats.time.updateGrid, frameStat->stats.time.updateGrid);
			UpdateMax(demoStat.max.stats.time.viscosityForces, frameStat->stats.time.viscosityForces);

			Accumulate(demoStat.avg.simulationTime, frameStat->simulationTime);
			Accumulate(demoStat.avg.stats.time.collisions, frameStat->stats.time.collisions);
			Accumulate(demoStat.avg.stats.time.deltaPositions, frameStat->stats.time.deltaPositions);
			Accumulate(demoStat.avg.stats.time.densityAndPressure, frameStat->stats.time.densityAndPressure);
			Accumulate(demoStat.avg.stats.time.emitters, frameStat->stats.time.emitters);
			Accumulate(demoStat.avg.stats.time.integration, frameStat->stats.time.integration);
			Accumulate(demoStat.avg.stats.time.neighborSearch, frameStat->stats.time.neighborSearch);
			Accumulate(demoStat.avg.stats.time.predict, frameStat->stats.time.predict);
			Accumulate(demoStat.avg.stats.time.updateGrid, frameStat->stats.time.updateGrid);
			Accumulate(demoStat.avg.stats.time.viscosityForces, frameStat->stats.time.viscosityForces);

			++avgCount;
		}
	}

	if (avgCount > 0) {
		const float avg = 1.0f / (float)avgCount;
		demoStat.avg.simulationTime *= avg;
		demoStat.avg.stats.time.collisions *= avg;
		demoStat.avg.stats.time.deltaPositions *= avg;
		demoStat.avg.stats.time.densityAndPressure *= avg;
		demoStat.avg.stats.time.emitters *= avg;
		demoStat.avg.stats.time.integration *= avg;
		demoStat.avg.stats.time.neighborSearch *= avg;
		demoStat.avg.stats.time.predict *= avg;
		demoStat.avg.stats.time.updateGrid *= avg;
		demoStat.avg.stats.time.viscosityForces *= avg;
	}

	demoStat.frameCount = maxFrameCount;
	demoStat.iterationCount = iterationCount;
	demoStats.push_back(demoStat);
}

void DemoApplication::RenderBenchmark(OSDState *osdState, const float width, const float height) {
	std::string processorName = GetProcessorName();

	Chart chart = Chart();
	chart.axisFormat = "%.2f ms";
	chart.AddSampleLabel("Total");
	chart.AddSampleLabel("Integration");
	chart.AddSampleLabel("Viscosity");
	chart.AddSampleLabel("Predict");
	chart.AddSampleLabel("Grid");
	chart.AddSampleLabel("Neighbors");
	chart.AddSampleLabel("Pressure");
	chart.AddSampleLabel("Delta");
	chart.AddSampleLabel("Collisions");

	RandomSeries colorRandomSeries = RandomSeed(1337);
	for (size_t seriesIndex = 0; seriesIndex < demoStats.size(); ++seriesIndex) {
		DemoStatistics *demoStat = &demoStats[seriesIndex];
		ChartSeries series = ChartSeries();
		series.color = RandomColor(&colorRandomSeries);
		series.title = std::string("Demo ") + std::to_string(seriesIndex + 1);
		FrameStatistics *frameStats = &demoStat->max;
		series.AddValue(frameStats->simulationTime);
		series.AddValue(frameStats->stats.time.integration);
		series.AddValue(frameStats->stats.time.viscosityForces);
		series.AddValue(frameStats->stats.time.predict);
		series.AddValue(frameStats->stats.time.updateGrid);
		series.AddValue(frameStats->stats.time.neighborSearch);
		series.AddValue(frameStats->stats.time.densityAndPressure);
		series.AddValue(frameStats->stats.time.deltaPositions);
		series.AddValue(frameStats->stats.time.collisions);
		chart.AddSeries(series);
	}

	float viewport[4] = {0.0f, 0.0f, width, height - (osdState->fontHeight * 2.0f)};
	chart.RenderBars(viewport, osdState->font, (float)osdState->fontHeight);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	char osdBuffer[256];
	DemoStatistics *firstDemoStat = &demoStats[0];
	sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Benchmark done, Scenario: %llu, Frames: %llu, Iterations: %llu", (firstDemoStat->scenarioIndex + 1), firstDemoStat->frameCount, firstDemoStat->iterationCount);
	DrawOSDLine(osdState, osdBuffer);
	sprintf_s(osdBuffer, ArrayCount(osdBuffer), "%s", processorName.c_str());
	DrawOSDLine(osdState, osdBuffer);
}

void DemoApplication::UpdateAndRender(const float frameTime, const uint64_t cycles) {
	if (simulationActive) {
		externalForcesApplying = false;

		float updateTime = 0.0f;
		{
			auto startClock = std::chrono::high_resolution_clock::now();
			for (int step = 0; step < kSPHSubsteps; ++step) {
				demo->Update(kSPHSubstepDeltaTime);
			}
			auto deltaClock = std::chrono::high_resolution_clock::now() - startClock;
			updateTime = std::chrono::duration_cast<std::chrono::nanoseconds>(deltaClock).count() * nanosToMilliseconds;
		}

		if (benchmarkActive) {
			assert(activeBenchmarkIteration != nullptr);
			activeBenchmarkIteration->frames.push_back(FrameStatistics(demo->GetStats(), updateTime));
			++benchmarkFrameCount;

			if (activeBenchmarkIteration->frames.size() == kBenchmarkFrameCount) {
				// Iteration complete
				if (benchmarkIterations.size() == kBenchmarkIterationCount) {

					// Calculate and add demo statistics
					PushDemoStatistics();

					// Demo complete
					if (demoIndex == (kDemoCount - 1)) {
						// Benchmark complete
						benchmarkFrameCount = 0;
						simulationActive = false;
						benchmarkDone = true;
						benchmarkActive = false;
						activeBenchmarkIteration = nullptr;
					} else {
						// Next demo
						demoIndex++;
						LoadDemo(demoIndex);

						benchmarkIterations.clear();
						benchmarkIterations.push_back(BenchmarkIteration(kBenchmarkFrameCount));
						activeBenchmarkIteration = &benchmarkIterations[0];
					}
				} else {
					// Next iteration
					benchmarkIterations.push_back(BenchmarkIteration(kBenchmarkFrameCount));
					activeBenchmarkIteration = &benchmarkIterations[benchmarkIterations.size() - 1];
					LoadScenario(activeScenarioIndex);
				}
			}
		}
	}

	float left = -kSPHBoundaryHalfWidth;
	float right = kSPHBoundaryHalfWidth;
	float top = kSPHBoundaryHalfHeight;
	float bottom = -kSPHBoundaryHalfHeight;

	int w = window->width;
	int h = window->height;
	glViewport(0, 0, w, h);

	float worldToScreenScale = (float)w / kSPHBoundaryWidth;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(left, right, bottom, top, 0.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (!benchmarkDone) {
		demo->Render(worldToScreenScale);
	}

	char osdBuffer[256];
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, w, 0, h, 0.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// OSD
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	OSDState osdState = CreateOSD(GLUT_BITMAP_8_BY_13);
	osdState.charY = h - osdState.fontHeight;

	if (!benchmarkActive) {
		if (benchmarkDone && (demoStats.size() > 0)) {
			RenderBenchmark(&osdState, (float)w, (float)h);
		} else {
			size_t scenarioCount = ArrayCount(SPHScenarios);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Scenario: [%llu / %llu] %s (Space)", (activeScenarioIndex + 1), scenarioCount, activeScenarioName.c_str());
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Demo: %s (D)", demoTitle.c_str());
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Start benchmark (B)");
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Simulation: %s (P)", (simulationActive ? "yes" : "no"));
			DrawOSDLine(&osdState, osdBuffer);
			if (demo->IsMultiThreadingSupported()) {
				sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Multithreading: %s, %llu threads (T)", (demo->IsMultiThreading() ? "yes" : "no"), demo->GetWorkerThreadCount());
			} else {
				sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Multithreading: not supported");
			}
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Reset (R)");
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Frame time: %f ms, Cycles: %llu", (frameTime * 1000.0f), cycles);
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Particles: %llu", demo->GetParticleCount());
			DrawOSDLine(&osdState, osdBuffer);

			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Stats:");
			DrawOSDLine(&osdState, osdBuffer);
			const SPHStatistics &stats = demo->GetStats();
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "\tMin/Max cell particle count: %llu / %llu", stats.minCellParticleCount, stats.maxCellParticleCount);
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "\tMin/Max particle neighbor count: %llu / %llu", stats.minParticleNeighborCount, stats.maxParticleNeighborCount);
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "\tTime integration: %f ms", stats.time.integration);
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "\tTime viscosity forces: %f ms", stats.time.viscosityForces);
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "\tTime predict: %f ms", stats.time.predict);
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "\tTime update grid: %f ms", stats.time.updateGrid);
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "\tTime neighbor search: %f ms", stats.time.neighborSearch);
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "\tTime density and pressure: %f ms", stats.time.densityAndPressure);
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "\tTime delta positions: %f ms", stats.time.deltaPositions);
			DrawOSDLine(&osdState, osdBuffer);
			sprintf_s(osdBuffer, ArrayCount(osdBuffer), "\tTime collisions: %f ms", stats.time.collisions);
			DrawOSDLine(&osdState, osdBuffer);
		}
	} else {
		sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Benchmarking - Demo %llu of %llu, Scenario: %s (Escape)", demoIndex + 1, (size_t)4, activeScenarioName.c_str());
		DrawOSDLine(&osdState, osdBuffer);
		sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Iteration %llu of %llu", benchmarkIterations.size(), kBenchmarkIterationCount);
		DrawOSDLine(&osdState, osdBuffer);
		assert(activeBenchmarkIteration != nullptr);
		sprintf_s(osdBuffer, ArrayCount(osdBuffer), "Frame %llu of %llu", activeBenchmarkIteration->frames.size() + 1, kBenchmarkFrameCount);
		DrawOSDLine(&osdState, osdBuffer);

		const char *bigText = "Benchmarking";
		float bigTextSize = 30.0f;
		float bigTextWidth = GetStrokeTextWidth(bigText, bigTextSize);
		float bigTextX = w * 0.5f - bigTextWidth * 0.5f;
		float bigTextY = h * 0.5f - bigTextSize * 0.5f;
		RenderStrokeText(bigTextX, h * 0.5f, bigText, Vec4f(1, 1, 1, 1), bigTextSize, 2.0f);

		float progressWidth = bigTextWidth;
		float progressHeight = bigTextSize * 0.5f;
		float progressLeft = (w - progressWidth) * 0.5f;
		float progressBottom = bigTextY - progressHeight;
		size_t totalFrames = kBenchmarkFrameCount * kBenchmarkIterationCount * kDemoCount;
		float framesPercentage = benchmarkFrameCount / (float)totalFrames;
		FillRectangle(Vec2f(progressLeft, progressBottom), Vec2f(progressWidth * framesPercentage, progressHeight), Vec4f(0.1f, 0.1f, 0.6f, 1));
		DrawRectangle(Vec2f(progressLeft, progressBottom), Vec2f(progressWidth, progressHeight), Vec4f(1, 1, 1, 1), 2.0f);
	}
}

void DemoApplication::LoadDemo(const size_t demoIndex) {
	if (demo != nullptr) {
		delete demo;
	}
	switch (demoIndex) {
		case 0:
		{
			demo = new Demo1::ParticleSimulation();
			demoTitle = Demo1::kDemoName;
		} break;
		case 1:
		{
			demo = new Demo2::ParticleSimulation();
			demoTitle = Demo2::kDemoName;
		} break;
		case 2:
		{
			demo = new Demo3::ParticleSimulation();
			demoTitle = Demo3::kDemoName;
		} break;
		case 3:
		{
			demo = new Demo4::ParticleSimulation();
			demoTitle = Demo4::kDemoName;
		} break;
		default:
			assert(false);
	}
	demo->SetMultiThreading(multiThreadingActive);
	LoadScenario(activeScenarioIndex);
}

void DemoApplication::StartBenchmark() {
	benchmarkActive = true;
	benchmarkDone = false;
	benchmarkFrameCount = 0;

	benchmarkIterations.clear();
	benchmarkIterations.push_back(BenchmarkIteration(kBenchmarkFrameCount));
	activeBenchmarkIteration = &benchmarkIterations[0];

	demoStats.clear();

	simulationActive = true;
	demoIndex = 0;
	LoadDemo(demoIndex);
}

void DemoApplication::StopBenchmark() {
	benchmarkFrameCount = 0;
	simulationActive = false;
	benchmarkActive = false;
	benchmarkDone = true;
	activeBenchmarkIteration = nullptr;
}

void DemoApplication::KeyDown(unsigned char key) {
	if (!benchmarkActive) {
		if (!benchmarkDone && simulationActive) {
			bool doApplyingForces = false;
			Vec2f applyForceDirection = Vec2f(0, 0);

			if (key == GLUT_KEY_UP) {
				doApplyingForces = true;
				applyForceDirection += Vec2f(0, 1);
			} else if (key == GLUT_KEY_DOWN) {
				doApplyingForces = true;
				applyForceDirection += Vec2f(0, -1);
			}

			if (key == GLUT_KEY_LEFT) {
				doApplyingForces = true;
				applyForceDirection += Vec2f(-1, 0);
			} else if (key == GLUT_KEY_RIGHT) {
				doApplyingForces = true;
				applyForceDirection += Vec2f(1, 0);
			}

			if (doApplyingForces) {
				float strenth = 10.0f;
				externalForcesApplying = true;
				demo->AddExternalForces(applyForceDirection * strenth);
			}
		}
	}
}

void DemoApplication::KeyUp(unsigned char key) {
	if (!benchmarkActive) {
		if (benchmarkDone) {
			if (key == 27) {
				benchmarkDone = false;
			}
		} else {
			if (key == ' ') {
				activeScenarioIndex = (activeScenarioIndex + 1) % ArrayCount(SPHScenarios);
				LoadScenario(activeScenarioIndex);
			} else if (key == 'p') {
				simulationActive = !simulationActive;
			} else if (key == 'd') {
				demoIndex = (demoIndex + 1) % 4;
				simulationActive = true;
				LoadDemo(demoIndex);
			} else if (key == 'r') {
				LoadScenario(activeScenarioIndex);
			} else if (key == 't' && demo->IsMultiThreadingSupported()) {
				multiThreadingActive = !multiThreadingActive;
				demo->SetMultiThreading(multiThreadingActive);
			} else if (key == 'b') {
				StartBenchmark();
			}
		}
	} else {
		if (key == 27) {
			StopBenchmark();
		}
	}
}

void DemoApplication::LoadScenario(size_t scenarioIndex) {
	SPHScenario *scenario = &SPHScenarios[scenarioIndex];
	activeScenarioName = scenario->name;
	demo->ResetStats();
	demo->ClearBodies();
	demo->ClearParticles();
	demo->ClearEmitters();
	demo->SetGravity(scenario->gravity);
	demo->SetParams(scenario->parameters);

	// Bodies
	for (size_t bodyIndex = 0; bodyIndex < scenario->bodyCount; ++bodyIndex) {
		SPHScenarioBody *body = &scenario->bodies[bodyIndex];
		switch (body->type) {
			case SPHScenarioBodyType::SPHScenarioBodyType_Plane:
			{
				float distance = Vec2Dot(body->orientation.col1, body->position);
				demo->AddPlane(body->orientation.col1, distance);
			} break;
			case SPHScenarioBodyType::SPHScenarioBodyType_Circle:
			{
				demo->AddCircle(body->position, body->radius);
			} break;
			case SPHScenarioBodyType::SPHScenarioBodyType_LineSegment:
			{
				assert(body->vertexCount == 2);
				Vec2f a = Vec2MultMat2(body->orientation, body->localVerts[0]) + body->position;
				Vec2f b = Vec2MultMat2(body->orientation, body->localVerts[1]) + body->position;
				demo->AddLineSegment(a, b);
			} break;
			case SPHScenarioBodyType::SPHScenarioBodyType_Polygon:
			{
				assert(body->vertexCount >= 3);
				Vec2f verts[kMaxScenarioPolygonCount];
				for (size_t vertexIndex = 0; vertexIndex < body->vertexCount; ++vertexIndex) {
					verts[vertexIndex] = Vec2MultMat2(body->orientation, body->localVerts[vertexIndex]) + body->position;
				}
				demo->AddPolygon(body->vertexCount, verts);
			} break;
		}
	}

	// Volumes
	const SPHParameters &params = demo->GetParams();
	const float spacing = params.particleSpacing;
	for (size_t volumeIndex = 0; volumeIndex < scenario->bodyCount; ++volumeIndex) {
		SPHScenarioVolume *volume = &scenario->volumes[volumeIndex];
		int numX = (int)floor((volume->size.w / spacing));
		int numY = (int)floor((volume->size.h / spacing));
		demo->AddVolume(volume->position, volume->force, numX, numY, spacing);
	}

	// Emitters
	for (size_t emitterIndex = 0; emitterIndex < scenario->emitterCount; ++emitterIndex) {
		SPHScenarioEmitter *emitter = &scenario->emitters[emitterIndex];
		demo->AddEmitter(emitter->position, emitter->direction, emitter->radius, emitter->speed, emitter->rate, emitter->duration);
	}
}

#endif