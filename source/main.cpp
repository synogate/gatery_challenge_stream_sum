#include <gatery/frontend.h>
#include <gatery/utils.h>
#include <gatery/export/vhdl/VHDLExport.h>

#include <gatery/scl/arch/intel/IntelDevice.h>
#include <gatery/scl/synthesisTools/IntelQuartus.h>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/simulation/ReferenceSimulator.h>

#include <iostream>

using namespace gtry;
using namespace gtry::vhdl;
using namespace gtry::scl;
using namespace gtry::utils; 


/**
 * @brief Implement the challenge here
 * 
 * @param enable 
 * @return Bit 
 */
Bit challenge(Bit enable)
{
	hlim::ClockRational blinkFrequency{1, 1}; // 1Hz
	size_t counterMax = hlim::floor(ClockScope::getClk().absoluteFrequency() / blinkFrequency);
	UInt counter = BitWidth(utils::Log2C(counterMax+1));
	
	IF (enable)
		counter += 1;

	counter = reg(counter, 0);
	HCL_NAMED(counter);

	Bit ledOn = counter.msb();
	HCL_NAMED(ledOn);
	return ledOn;
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
	Clock clock{{.absoluteFrequency = 1'000}}; // 1KHz
	ClockScope clockScope{ clock };

	auto enable = pinIn().setName("button");
	
	auto ledOn = challenge(enable);

	pinOut(ledOn).setName("led");

	design.postprocess();

	// Setup simulation
	sim::ReferenceSimulator simulator;
	simulator.compileProgram(design.getCircuit());

	simulator.addSimulationProcess([=, &clock]()->SimProcess{

		fork([=, &clock]()->SimProcess{
			while (true) {
				co_await OnClk(clock);

				if (simu(ledOn) == '1')
					std::cout << "LED is on" << std::endl;
				else if (simu(ledOn) == '0')
					std::cout << "LED is off" << std::endl;
				else
					std::cout << "LED is undefined" << std::endl;
			}
		});


		std::cout << "Disabling" << std::endl;
		simu(enable) = '0';
		for ([[maybe_unused]]auto i : Range(50))
			co_await AfterClk(clock);

		std::cout << "Enabling" << std::endl;
		simu(enable) = '1';
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
	simulator.advance(hlim::ClockRational(5000,1'000));

	return 0;
}