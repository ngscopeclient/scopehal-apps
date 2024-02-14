function(add_compute_shaders target)
	cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES")

	set(spvfiles "")

	foreach(source ${arg_SOURCES})
		get_filename_component(base ${source} NAME_WE)

		set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${base}.spv)
		set(spvfiles ${spvfiles} ${outfile})

		add_custom_command(
			OUTPUT ${outfile}
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source}
			COMMENT "Compile shader ${base}"
			COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.0 -c ${CMAKE_CURRENT_SOURCE_DIR}/${source} -g -o ${outfile})

		install(FILES ${outfile} DESTINATION share/ngscopeclient/shaders)

	endforeach()

	add_custom_target(${target}
		COMMAND ${CMAKE_COMMAND} -E true
		SOURCES ${spvfiles}
	)

endfunction()

add_compute_shaders(
	ngcomputeshaders
	SOURCES
		ConstellationToneMap.glsl
		EyeToneMap.glsl
		ScopeDeskewUniform4xRate.glsl
		ScopeDeskewUniformUnequalRate.glsl
		ScopeDeskewUniformEqualRate.glsl
		SpectrogramToneMap.glsl
		WaterfallToneMap.glsl
		WaveformToneMap.glsl
	)

function(add_render_shader_variants target)
	cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "OUTPUTS")

	set(spvfiles "")

	set(source waveform-compute.glsl)
	foreach(outfn ${arg_OUTPUTS})
		set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${outfn})
		set(spvfiles ${spvfiles} ${outfile})

		#Compilation options are decided based on filename:
		set(options "")
		if(outfn MATCHES "analog")
			set(options ${options} -DANALOG_PATH)
		elseif(outfn MATCHES "digital")
			set(options ${options} -DDIGITAL_PATH)
		elseif(outfn MATCHES "histogram")
			set(options ${options} -DANALOG_PATH -DNO_INTERPOLATION -DHISTOGRAM_PATH)
		endif()

		if(outfn MATCHES "int64")
			set(options ${options} -DHAS_INT64)
		endif()

		if(outfn MATCHES "dense")
			set(options ${options} -DDENSE_PACK)
		endif()

		if(outfn MATCHES "zerohold")
			set(options ${options} -DNO_INTERPOLATION)
		endif()

		add_custom_command(
			OUTPUT ${outfile}
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source}
			COMMENT "Compile ${outfile} with ${options}"
			COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.0 -c ${CMAKE_CURRENT_SOURCE_DIR}/${source} ${options} -g -o ${outfile})

			install(FILES ${outfile} DESTINATION share/ngscopeclient/shaders)

	endforeach()

	add_custom_target(${target}
		COMMAND ${CMAKE_COMMAND} -E true
		SOURCES ${spvfiles}
	)

endfunction()

add_render_shader_variants(
	ngrendershaders
	OUTPUTS
		waveform-compute.analog.spv
		waveform-compute.analog.zerohold.spv
		waveform-compute.digital.spv
		waveform-compute.histogram.spv
		waveform-compute.analog.int64.spv
		waveform-compute.analog.zerohold.int64.spv
		waveform-compute.digital.int64.spv
		waveform-compute.histogram.int64.spv
		waveform-compute.analog.dense.spv
		waveform-compute.analog.zerohold.dense.spv
		waveform-compute.digital.dense.spv
		waveform-compute.histogram.dense.spv
		waveform-compute.analog.int64.dense.spv
		waveform-compute.analog.zerohold.int64.dense.spv
		waveform-compute.digital.int64.dense.spv
		waveform-compute.histogram.int64.dense.spv
	)

add_dependencies(ngscopeclient
	ngrendershaders
	ngcomputeshaders
	)
