#include <gatery/frontend.h>
#include <gatery/utils.h>
#include <gatery/export/vhdl/VHDLExport.h>

#include <gatery/scl/arch/intel/IntelDevice.h>
#include <gatery/scl/synthesisTools/IntelQuartus.h>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/simulation/ReferenceSimulator.h>

#include <gatery/scl/stream/Stream.h>


#include <iostream>

using namespace gtry;
using namespace gtry::vhdl;
using namespace gtry::scl;
using namespace gtry::utils; 


/**
 * @brief Implement the challenge here.
 * @details This function receives a stream of unsigned integer numbers with ready-valid handshake signals.
 * The function is to sum N consecutive numbers (where N is a construction time / generic parameter) and output these sums
 * as an output stream, again with ready-valid handshake signals.
 * So for every N transfers on the input stream there is to be one transfer (of the sum) on the output stream.
 *
 * The input stream may become in-valid at any point, even within the burst of integers to be summed.
 * The output stream may become un-ready at any point as well.
 */
RvStream<UInt> sum_N_numbers(RvStream<UInt> &inStream, size_t N)
{
	HCL_NAMED(inStream);

	RvStream<UInt> outStream;

	// Simply connecting input and output stream to demonstrate accessors. This needs to be changed.

	*outStream = *inStream; // *stream returns the payload of the stream (the integer numbers). It is assignable.
	valid(outStream) = valid(inStream); // valid(...) returns the valid bit of the stream. It is assignable.
	ready(inStream) = ready(outStream); // ready(...) returns the ready bit of the stream. It is assignable.

	/*
		Other useful syntax constructs:

		`transfer(stream)` is a shorthand for `ready(stream) & valid(stream)`
		`stream->width()` calls width() on the underlying payload of the stream, i.e. returns the bit width of the integer.
	*/

	HCL_NAMED(outStream);
	return outStream;
}

int main()
{
	DesignScope design;

	if (true) {
		auto device = std::make_unique<IntelDevice>();
		device->setupCyclone10();
		design.setTargetTechnology(std::move(device));
	}

	// Build circuit
	Clock clock{{.absoluteFrequency = 1'000'000}};
	ClockScope clockScope{ clock };

	RvStream<UInt> inStream{ 8_b };
	pinIn(inStream, "input");

	const size_t N = 5;

	auto outStream = sum_N_numbers(inStream, N);

	pinOut(outStream, "output");

	design.postprocess();

	// Setup simulation
	sim::ReferenceSimulator simulator;
	simulator.compileProgram(design.getCircuit());

	std::queue<std::array<size_t, N>> data;
	simulator.addSimulationProcess([=, &data, &inStream, &outStream, &clock]()->SimProcess{

		std::mt19937 rng(1337);

		// Data generator
		fork([=, &inStream, &clock, &data, &rng]()->SimProcess{
			co_await OnClk(clock);

			while (true) {
				std::array<size_t, N> elems;
				for (auto i : Range(N)) {
					elems[i] = rng() % 256;
					simu(*inStream) = elems[i];

					co_await scl::performTransferWait(inStream, clock);
				}

				data.push(elems);
			}
		});

		// Chaos monkey on ready and valid
		fork([=, &inStream, &outStream, &clock, &data, &rng]()->SimProcess{
			simu(valid(inStream)) = '0';
			simu(ready(outStream)) = '0';
			while (true) {
				co_await OnClk(clock);
				simu(valid(inStream)) = (rng() & 1) == 1;
				simu(ready(outStream)) = (rng() & 1) == 1;
			}
		});

		// Actually check the output
		while (true) {
			co_await scl::performTransferWait(outStream, clock);

			if (data.empty())
				std::cerr << "Output returned sum but no complete tuple was inserted for it at " << toNanoseconds(getCurrentSimulationTime()) << " ns." << std::endl;
			else {
				auto last = data.back();
				data.pop();

				size_t expectedSum = 0;
				for (auto s : last)
					expectedSum += s;

				if (simu(*outStream) != expectedSum)
					std::cerr << "Output returned the wrong sum at " << toNanoseconds(getCurrentSimulationTime()) << " ns." << std::endl;
			}
		}
	});

	// Record simulation waveforms as VCD file
	sim::VCDSink vcd(design.getCircuit(), simulator, "waveform.vcd");
	vcd.addAllPins();
	vcd.addAllSignals();


	if (true) {
		// VHDL export
		VHDLExport vhdl("vhdl/");
		vhdl.targetSynthesisTool(new IntelQuartus());
		vhdl.writeProjectFile("import_IPCore.tcl");
		vhdl.writeStandAloneProjectFile("IPCore.qsf");
		vhdl.writeConstraintsFile("constraints.sdc");
		vhdl.writeClocksFile("clocks.sdc");
		vhdl(design.getCircuit());
	}

	// Run simulation
	simulator.powerOn();
	simulator.advance(hlim::ClockRational(200,1'000'000));

	if (data.size() > 1)
		std::cerr << "Insufficient sums returned." << std::endl;

	return 0;
}