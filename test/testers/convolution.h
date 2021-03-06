#pragma once

#include <cstddef>
#include <cstdlib>

#include <cmath>
#include <cfloat>
#include <vector>
#include <random>
#include <chrono>
#include <functional>
#include <algorithm>

#include <nnpack.h>
#include <nnpack/reference.h>
#include <nnpack/AlignedAllocator.h>

#include <testers/relu.h>

class ConvolutionTester {
public:
	ConvolutionTester() :
		iterations_(1),
		errorLimit_(1.0e-5f),
		multithreading_(false),
		batchSize_(1),
		inputChannels_(1),
		outputChannels_(1)
	{
		inputSize(4, 4);
		kernelSize(3, 3);
		inputPadding(0, 0, 0, 0);
		outputSubsampling(1, 1);		
	}

	ConvolutionTester(const ConvolutionTester&) = delete;

	inline ConvolutionTester(ConvolutionTester&& tester) :
		iterations_(tester.iterations_),
		errorLimit_(tester.errorLimit_),
		multithreading_(tester.multithreading_),
		batchSize_(tester.batchSize_),
		inputChannels_(tester.inputChannels_),
		outputChannels_(tester.outputChannels_),
		inputSize_(tester.inputSize_),
		inputPadding_(tester.inputPadding_),
		kernelSize_(tester.kernelSize_),
		outputSubsampling_(tester.outputSubsampling_)
		
	{
		
	}

	ConvolutionTester& operator=(const ConvolutionTester&) = delete;

	~ConvolutionTester() {
		
	}

	inline ConvolutionTester& iterations(size_t iterations) {
		this->iterations_ = iterations;
		return *this;
	}

	inline size_t iterations() const {
		return this->iterations_;
	}

	inline ConvolutionTester& errorLimit(float errorLimit) {
		this->errorLimit_ = errorLimit;
		return *this;
	}

	inline float errorLimit() const {
		return this->errorLimit_;
	}

	inline ConvolutionTester& multithreading(bool multithreading) {
		this->multithreading_ = multithreading;
		
		return *this;
	}

	inline bool multithreading() const {
		return this->multithreading_;
	}

	inline ConvolutionTester& batchSize(size_t batchSize) {
		this->batchSize_ = batchSize;
		return *this;
	}

	inline size_t batchSize() const {
		return this->batchSize_;
	}

	inline ConvolutionTester& inputChannels(size_t inputChannels) {
		this->inputChannels_ = inputChannels;
		return *this;
	}

	inline size_t inputChannels() const {
		return this->inputChannels_;
	}

	inline ConvolutionTester& outputChannels(size_t outputChannels) {
		this->outputChannels_ = outputChannels;
		return *this;
	}

	inline size_t outputChannels() const {
		return this->outputChannels_;
	}

	inline ConvolutionTester& inputSize(size_t height, size_t width) {
		this->inputSize_.height = height;
		this->inputSize_.width = width;
		return *this;
	}

	inline struct nnp_size inputSize() const {
		return this->inputSize_;
	}

	inline size_t inputHeight() const {
		return this->inputSize_.height;
	}

	inline size_t inputWidth() const {
		return this->inputSize_.width;
	}

	inline ConvolutionTester& kernelSize(size_t height, size_t width) {
		this->kernelSize_.height = height;
		this->kernelSize_.width = width;
		return *this;
	}

	inline struct nnp_size kernelSize() const {
		return this->kernelSize_;
	}

	inline size_t kernelHeight() const {
		return this->kernelSize_.height;
	}

	inline size_t kernelWidth() const {
		return this->kernelSize_.width;
	}

	inline struct nnp_size outputSize() const {
		struct nnp_size outputSize;
		outputSize.height = this->outputHeight();
		outputSize.width = this->outputWidth();
		return outputSize;
	}

	inline size_t outputHeight() const {
		return (this->inputPadding_.top + this->inputSize_.height + this->inputPadding_.bottom - this->kernelSize_.height) / this->outputSubsampling_.height + 1;
	}

	inline size_t outputWidth() const {
		return (this->inputPadding_.left + this->inputSize_.width + this->inputPadding_.right - this->kernelSize_.width) / this->outputSubsampling_.width + 1;
	}

	inline ConvolutionTester& outputSubsampling(size_t height, size_t width) {
		this->outputSubsampling_.height = height;
		this->outputSubsampling_.width = width;
		return *this;
	}

	inline struct nnp_size outputSubsampling() const {
		return this->outputSubsampling_;
	}

	inline ConvolutionTester& inputPadding(size_t top, size_t right, size_t bottom, size_t left) {
		this->inputPadding_.top = top;
		this->inputPadding_.right = right;
		this->inputPadding_.bottom = bottom;
		this->inputPadding_.left = left;
		return *this;
	}

	inline struct nnp_padding inputPadding() const {
		return this->inputPadding_;
	}

	void testOutput(enum nnp_convolution_algorithm algorithm, enum nnp_activation activation = nnp_activation_identity) const {
		const uint_fast32_t seed = uint_fast32_t(std::chrono::system_clock::now().time_since_epoch().count());
		auto rng = std::bind(std::uniform_real_distribution<float>(-0.1f, 1.0f), std::mt19937(seed));

		std::vector<float> input(batchSize() * inputChannels() * inputHeight() * inputWidth());
		std::vector<float> kernel(outputChannels() * inputChannels() * kernelHeight() * kernelWidth());

		std::vector<float> bias(outputChannels());

		std::vector<float> output(batchSize() * outputChannels() * outputHeight() * outputWidth());
		std::vector<float> referenceOutput(batchSize() * outputChannels() * outputHeight() * outputWidth());

		size_t scratchSize = 0;
		enum nnp_status status = nnp_convolution_output(
			algorithm,
			batchSize(), inputChannels(), outputChannels(),
			inputSize(), inputPadding(), kernelSize(),
			nullptr, nullptr, nullptr, nullptr, nullptr, &scratchSize,
			activation, nullptr,
			nullptr);
		ASSERT_EQ(nnp_status_success, status);

		std::vector<uint8_t, AlignedAllocator<uint8_t, 64>> scratchBuffer(scratchSize);
		std::vector<float> maxErrors;
		for (size_t iteration = 0; iteration < iterations(); iteration++) {
			std::generate(input.begin(), input.end(), std::ref(rng));
			std::generate(kernel.begin(), kernel.end(), std::ref(rng));
			std::generate(bias.begin(), bias.end(), std::ref(rng));
			std::fill(output.begin(), output.end(), nanf(""));
			std::fill(scratchBuffer.begin(), scratchBuffer.end(), 0xA5);

			nnp_convolution_output__reference(
				batchSize(), inputChannels(), outputChannels(),
				inputSize(), inputPadding(), kernelSize(), outputSubsampling(),
				input.data(), kernel.data(), bias.data(), referenceOutput.data());

			switch (activation) {
				case nnp_activation_identity:
					break;
				case nnp_activation_relu:
					nnp_relu_output__reference(
						batchSize(), outputChannels() * outputHeight() * outputWidth(),
						referenceOutput.data(), referenceOutput.data(), 0.0f);
					break;
				default:
					FAIL() << "Unexpected activation value: " << activation;
			}

			enum nnp_status status = nnp_convolution_output(
				algorithm,
				batchSize(), inputChannels(), outputChannels(),
				inputSize(), inputPadding(), kernelSize(),
				input.data(), kernel.data(), bias.data(), output.data(),
				scratchSize == 0 ? nullptr : scratchBuffer.data(),
				scratchSize == 0 ? nullptr : &scratchSize,
				activation, nullptr,
				nullptr);
			ASSERT_EQ(nnp_status_success, status);

			const float maxError = std::inner_product(referenceOutput.cbegin(), referenceOutput.cend(), output.cbegin(), 0.0f,
				[](float x, float y)->float { return std::max<float>(y, x); }, relativeError);
			maxErrors.push_back(maxError);
		}
		EXPECT_LT(median(maxErrors), errorLimit());
	}

	void testInputGradient(enum nnp_convolution_algorithm algorithm, enum nnp_activation activation = nnp_activation_identity) const {
		const uint_fast32_t seed = uint_fast32_t(std::chrono::system_clock::now().time_since_epoch().count());
		auto rng = std::bind(std::uniform_real_distribution<float>(), std::mt19937(seed));

		std::vector<float> outputGradient(batchSize() * outputChannels() * outputHeight() * outputWidth());
		std::vector<float> kernel(outputChannels() * inputChannels() * kernelHeight() * kernelWidth());

		std::vector<float> inputGradient(batchSize() * inputChannels() * inputHeight() * inputWidth());

		std::vector<float> referenceInputGradient(batchSize() * inputChannels() * inputHeight() * inputWidth());

		size_t scratchSize = 0;
		enum nnp_status status = nnp_convolution_input_gradient(
			algorithm,
			batchSize(), inputChannels(), outputChannels(),
			inputSize(), inputPadding(), kernelSize(),
			nullptr, nullptr, nullptr, nullptr, &scratchSize,
			nnp_activation_identity, nullptr,
			nullptr);
		ASSERT_EQ(nnp_status_success, status);

		std::vector<uint8_t, AlignedAllocator<uint8_t, 64>> scratchBuffer(scratchSize);
		std::vector<float> maxErrors;
		for (size_t iteration = 0; iteration < iterations(); iteration++) {
			std::generate(outputGradient.begin(), outputGradient.end(), std::ref(rng));
			std::generate(kernel.begin(), kernel.end(), std::ref(rng));
			std::fill(inputGradient.begin(), inputGradient.end(), nanf(""));
			std::fill(scratchBuffer.begin(), scratchBuffer.end(), 0xA5);

			nnp_convolution_input_gradient__reference(
				batchSize(), inputChannels(), outputChannels(),
				inputSize(), inputPadding(), kernelSize(),
				outputGradient.data(), kernel.data(), referenceInputGradient.data());

			enum nnp_status status = nnp_convolution_input_gradient(
				algorithm,
				batchSize(), inputChannels(), outputChannels(),
				inputSize(), inputPadding(), kernelSize(),
				outputGradient.data(), kernel.data(), inputGradient.data(),
				scratchSize == 0 ? nullptr : scratchBuffer.data(),
				scratchSize == 0 ? nullptr : &scratchSize,
				nnp_activation_identity, NULL,
				nullptr);
			ASSERT_EQ(nnp_status_success, status);

			const float maxError = std::inner_product(referenceInputGradient.cbegin(), referenceInputGradient.cend(), inputGradient.cbegin(), 0.0f,
				[](float x, float y)->float { return std::max<float>(y, x); }, relativeError);
			maxErrors.push_back(maxError);
		}
		EXPECT_LT(median(maxErrors), errorLimit());
	}

	void testKernelGradient(enum nnp_convolution_algorithm algorithm, enum nnp_activation activation = nnp_activation_identity) const {
		const uint_fast32_t seed = uint_fast32_t(std::chrono::system_clock::now().time_since_epoch().count());
		auto rng = std::bind(std::uniform_real_distribution<float>(), std::mt19937(seed));

		std::vector<float> input(batchSize() * inputChannels() * inputHeight() * inputWidth());
		std::vector<float> outputGradient(batchSize() * outputChannels() * outputHeight() * outputWidth());
		std::vector<float> kernelGradient(outputChannels() * inputChannels() * kernelHeight() * kernelWidth());

		std::vector<float> referenceKernelGradient(outputChannels() * inputChannels() * kernelHeight() * kernelWidth());
		
		size_t scratchSize = 0;
		enum nnp_status status = nnp_convolution_kernel_gradient(
			algorithm,
			batchSize(), inputChannels(), outputChannels(),
			inputSize(), inputPadding(), kernelSize(),
			nullptr, nullptr, nullptr, nullptr, &scratchSize,
			nnp_activation_identity, nullptr,
			nullptr);
		ASSERT_EQ(nnp_status_success, status);

		std::vector<uint8_t, AlignedAllocator<uint8_t, 64>> scratchBuffer(scratchSize);
		std::vector<float> maxErrors;
		for (size_t iteration = 0; iteration < iterations(); iteration++) {
			std::generate(input.begin(), input.end(), std::ref(rng));
			std::generate(outputGradient.begin(), outputGradient.end(), std::ref(rng));
			std::fill(kernelGradient.begin(), kernelGradient.end(), nanf(""));
			std::fill(scratchBuffer.begin(), scratchBuffer.end(), 0xA5);

			nnp_convolution_kernel_gradient__reference(
				batchSize(), inputChannels(), outputChannels(),
				inputSize(), inputPadding(), kernelSize(),
				input.data(), outputGradient.data(), referenceKernelGradient.data());

			enum nnp_status status = nnp_convolution_kernel_gradient(
				algorithm,
				batchSize(), inputChannels(), outputChannels(),
				inputSize(), inputPadding(), kernelSize(),
				input.data(), outputGradient.data(), kernelGradient.data(),
				scratchSize == 0 ? nullptr : scratchBuffer.data(),
				scratchSize == 0 ? nullptr : &scratchSize,
				nnp_activation_identity, NULL,
				NULL);
			ASSERT_EQ(nnp_status_success, status);

			const float maxError = std::inner_product(referenceKernelGradient.cbegin(), referenceKernelGradient.cend(), kernelGradient.cbegin(), 0.0f,
				[](float x, float y)->float { return std::max<float>(y, x); }, relativeError);
			maxErrors.push_back(maxError);
		}
		EXPECT_LT(median(maxErrors), errorLimit());
	}

	void testInference(enum nnp_convolution_algorithm algorithm, enum nnp_activation activation = nnp_activation_identity, bool precompute = false) const {
		ASSERT_EQ(1, batchSize());

		const uint_fast32_t seed = uint_fast32_t(std::chrono::system_clock::now().time_since_epoch().count());
		auto rng = std::bind(std::uniform_real_distribution<float>(-0.1f, 1.0f), std::mt19937(seed));

		std::vector<float> input(inputChannels() * inputHeight() * inputWidth());
		std::vector<float> kernel(outputChannels() * inputChannels() * kernelHeight() * kernelWidth());

		std::vector<float> bias(outputChannels());

		std::vector<float> output(outputChannels() * outputHeight() * outputWidth());
		std::vector<float> referenceOutput(outputChannels() * outputHeight() * outputWidth());

		size_t scratchSize = 0;
		enum nnp_status status = nnp_convolution_inference(
			algorithm,
			precompute ? nnp_convolution_transform_strategy_reuse : nnp_convolution_transform_strategy_compute,
			inputChannels(), outputChannels(),
			inputSize(), inputPadding(), kernelSize(), outputSubsampling(),
			nullptr, nullptr, nullptr, nullptr, nullptr, &scratchSize,
			activation, nullptr,
			nullptr);
		ASSERT_EQ(nnp_status_success, status);

		std::vector<uint8_t, AlignedAllocator<uint8_t, 64>> scratchBuffer(scratchSize);

		std::vector<float> maxErrors;
		for (size_t iteration = 0; iteration < iterations(); iteration++) {
			std::generate(input.begin(), input.end(), std::ref(rng));
			std::generate(kernel.begin(), kernel.end(), std::ref(rng));
			std::generate(bias.begin(), bias.end(), std::ref(rng));
			std::fill(output.begin(), output.end(), nanf(""));
			std::fill(scratchBuffer.begin(), scratchBuffer.end(), 0xA5);

			nnp_convolution_output__reference(
				1, inputChannels(), outputChannels(),
				inputSize(), inputPadding(), kernelSize(), outputSubsampling(),
				input.data(), kernel.data(), bias.data(), referenceOutput.data());

			switch (activation) {
				case nnp_activation_identity:
					break;
				case nnp_activation_relu:
					nnp_relu_output__reference(
						batchSize(), outputChannels() * outputHeight() * outputWidth(),
						referenceOutput.data(), referenceOutput.data(), 0.0f);
					break;
				default:
					FAIL() << "Unexpected activation value: " << activation;
			
			}

			std::vector<uint8_t, AlignedAllocator<uint8_t, 64>> transformedKernel;

			if (precompute) {
				size_t transformedKernelSize = 0;
				enum nnp_status status = nnp_convolution_inference(
					algorithm, nnp_convolution_transform_strategy_precompute,
					inputChannels(), outputChannels(),
					inputSize(), inputPadding(), kernelSize(), outputSubsampling(),
					nullptr, nullptr, nullptr, nullptr, nullptr, &transformedKernelSize,
					activation, nullptr,
					nullptr);
				ASSERT_EQ(nnp_status_success, status);

				transformedKernel.resize(transformedKernelSize);

				status = nnp_convolution_inference(
					algorithm, nnp_convolution_transform_strategy_precompute,
					inputChannels(), outputChannels(),
					inputSize(), inputPadding(), kernelSize(), outputSubsampling(),
					nullptr, kernel.data(), nullptr, nullptr, transformedKernel.data(), &transformedKernelSize,
					activation, nullptr,
					nullptr);
				ASSERT_EQ(nnp_status_success, status);
			}

			const void* kernelData = kernel.data();
			if (precompute) {
				kernelData = transformedKernel.data();
			}

			enum nnp_status status = nnp_convolution_inference(
				algorithm,
				precompute ? nnp_convolution_transform_strategy_reuse : nnp_convolution_transform_strategy_compute,
				inputChannels(), outputChannels(),
				inputSize(), inputPadding(), kernelSize(), outputSubsampling(),
				input.data(), static_cast<const float*>(kernelData), bias.data(), output.data(),
				scratchSize == 0 ? nullptr : scratchBuffer.data(),
				scratchSize == 0 ? nullptr : &scratchSize,
				activation, nullptr,
			    nullptr);
			ASSERT_EQ(nnp_status_success, status);

			const float maxError = std::inner_product(referenceOutput.cbegin(), referenceOutput.cend(), output.cbegin(), 0.0f,
				[](float x, float y)->float { return std::max<float>(y, x); }, relativeError);
			maxErrors.push_back(maxError);
		}

		EXPECT_LT(median(maxErrors), errorLimit());
	}

private:
	inline static float relativeError(float reference, float actual) {
		return std::abs(reference - actual) / std::max(FLT_MIN, std::abs(reference));
	}

	inline static float median(std::vector<float>& array) {
		std::nth_element(array.begin(), array.begin() + array.size() / 2, array.end());
		return array[array.size() / 2];
	}

	size_t iterations_;
	float errorLimit_;
	bool multithreading_;

	size_t batchSize_;
	size_t inputChannels_;
	size_t outputChannels_;
	struct nnp_size inputSize_;
	struct nnp_padding inputPadding_;
	struct nnp_size kernelSize_;
	struct nnp_size outputSubsampling_;
};
