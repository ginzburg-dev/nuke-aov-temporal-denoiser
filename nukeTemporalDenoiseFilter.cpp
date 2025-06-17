#include "DDImage/Iop.h"
#include "DDImage/NukeWrapper.h"
#include "DDImage/Row.h"
#include "DDImage/Tile.h"
#include "DDImage/Pixel.h"
#include "DDImage/Filter.h"
#include "DDImage/Knobs.h"
#include "DDImage/Vector2.h"
#include "DDImage/DDMath.h"
#include "DDImage/MultiTile.h"
#include <algorithm>
#include <iostream>
#include <fstream>

using namespace DD::Image;

class GinzburgDenoiseFilterPlugin : public Iop
{
	int _size;
	float MotionVectorMult;
	float _wB;
	float _wA,_wAt;
	float _wD,_wP;
	float _wN;
	int xMax, yMax;
	float _wDist, _wColor, _wPosition;
	float _epsX,_epsY,_epsZ,_x,_y,_eps,_epsColor;
	bool useMV;
	float _wT,_wS;
	int nFrames, kernelRadius, searchRadius;
	bool _albedoDivide;
	bool _lic;
	Channel _mv[2];
	Channel _depth[1];
	Channel _position[3];
	Channel _beauty[3];
	Channel _normal[3];
	Channel _albedo[3];
	Channel _extraChannel[4][3];

public:
	void _validate(bool);
	void _request(int x, int y, int r, int t, ChannelMask channels, int count);
	const OutputContext& inputContext(int, int, OutputContext&) const;
	int maximum_inputs() const { return 1; }
	int minimum_inputs() const { return 1; }
	int split_input(int n) const { return 7; }

	//! Constructor. Initialize user controls to their default values.
	ExamplePlugin (Node* node) : Iop (node)
	{
		_lic = true;
		_size = 2;
		_wB = 1;
		_wAt = 0.01;
		_wT = 0.3f;
		_wDist = 0.2f;
		_epsColor = 0.01;
		_wColor = 0.2f;
		_epsX = _epsY = _epsZ = 1.0f;
		_albedoDivide = true;
		kernelRadius = 5;
		searchRadius = 3;
		MotionVectorMult = 1.0f;
		nFrames = 7;
	}

	~GinzburgDenoiseFilterPlugin () {}

	bool fexists(const std::string& filename) {
		std::ifstream ifile(filename.c_str());
		return (bool)ifile;
	}

	//! This function does all the work.
	void engine ( int y, int x, int r, ChannelMask channels, Row& outRow );

	virtual void knobs ( Knob_Callback f )
	{
		Float_knob(f, &MotionVectorMult, "MotionVectorMult", "MotionVectorMult");
		Tooltip(f, "Multiply the uv channels by this");
		Int_knob(f, &_size, "size", "Filter_size");
		Int_knob(f, &nFrames, "nFrames", "Frames");
		Tooltip(f, "Multiply the uv channels by this");
		Int_knob(f, &kernelRadius, " kernelRadius", "kernelRadius");
		Tooltip(f, "Multiply the uv channels by this");
		Int_knob(f, &searchRadius, " searchRadius", "searchRadius");
		Tooltip(f, "Multiply the uv channels by this");

		Float_knob(f, &_eps, "eps", "treshold");
		Tooltip(f, "Multiply the uv channels by this");
		Float_knob(f, &_epsColor, "eps1", "treshold color");
		Tooltip(f, "Multiply the uv channels by this");

		Float_knob(f, &_wPosition, "_wPosition", "sigma position");
		Tooltip(f, "Multiply the uv channels by this");
		Float_knob(f, &_wDist, "wDist", "sigma dist");
		Tooltip(f, "Multiply the uv channels by this");

		Float_knob(f, &_wColor, "wColor", "sigma color");
		Tooltip(f, "Multiply the uv channels by this");
		Float_knob(f, &_wAt, "sigma_albedo1", "sigma_albedo_temporal");
		Tooltip(f, "Multiply the uv channels by this");

		Bool_knob(f, &useMV, "useMV", "useMV");
		Tooltip(f, "Multiply the uv channels by this");
		Float_knob(f, &_wT, "temporal weight", "temporal weight");
		Tooltip(f, "Multiply the uv channels by this");
		Float_knob(f, &_wS, "spatial weight", "spatial weight");
		Tooltip(f, "Multiply the uv channels by this");
		Float_knob(f, &_wA, "sigma_albedo", "sigma_albedo");
		Tooltip(f, "Multiply the uv channels by this");
		Float_knob(f, &_wD, "sigma_depth", "sigma_depth");
		Tooltip(f, "Multiply the uv channels by this");
		Float_knob(f, &_wN, "sigma_normal", "sigma_normal");
		Tooltip(f, "Multiply the uv channels by this");
		Float_knob(f, &_wP, "sigma_position1", "sigma_position_second");
		Tooltip(f, "Multiply the uv channels by this");
		Float_knob(f, &_wB, "sigma_beauty", "sigma_color_second");
		Tooltip(f, "Multiply the uv channels by this");

		Input_Channel_knob ( f, _mv, 2, 0, "_mv", "MotionVector channel");
		Tooltip(f, "The values in these channels are added to the pixel "
				"coordinate to get the source pixel.");
		Input_Channel_knob ( f, _position, 3, 0, "_position", "Position channel");
		Tooltip(f, "The values in these channels are added to the pixel "
				"coordinate to get the source pixel.");
		Input_Channel_knob ( f, _beauty, 3, 0, "_beauty", "Beauty channel");
		Tooltip(f, "The values in these channels are added to the pixel "
				"coordinate to get the source pixel.");
		Input_Channel_knob ( f, _depth, 1, 0, "_depth", "Depth channel");
		Tooltip(f, "The values in these channels are added to the pixel "
				"coordinate to get the source pixel.");
		Input_Channel_knob ( f, _normal, 3, 0, "_normal", "Normal channel");
		Tooltip(f, "The values in these channels are added to the pixel "
				"coordinate to get the source pixel.");
		Input_Channel_knob ( f, _albedo, 3, 0, "_albedo", "Albedo channel");
		Tooltip(f, "The values in these channels are added to the pixel "
				"coordinate to get the source pixel.");

		Input_Channel_knob ( f, _extraChannel[0], 3, 0, "_extraChannel[0]", "Extra channel 0");
		Tooltip(f, "The values in these channels are added to the pixel "
				"coordinate to get the source pixel.");
		Input_Channel_knob ( f, _extraChannel[1], 3, 0, "_extraChannel[1]", "Extra channel 1");
		Tooltip(f, "The values in these channels are added to the pixel "
				"coordinate to get the source pixel.");
		Input_Channel_knob ( f, _extraChannel[2], 3, 0, "_extraChannel[2]", "Extra channel 2");
		Tooltip(f, "The values in these channels are added to the pixel "
				"coordinate to get the source pixel.");
		Input_Channel_knob ( f, _extraChannel[3], 3, 0, "_extraChannel[3]", "Extra channel 3");
		Tooltip(f, "The values in these channels are added to the pixel "
				"coordinate to get the source pixel.");
	}
	//! Return the name of the class.
	const char* Class() const { return CLASS; }
	const char* node_help() const { return HELP; }

private:
	//! Information to the plug-in manager of DDNewImage/NUKE.
	static const Iop::Description description;
	static const char* const CLASS;
	static const char* const HELP;
}; 

/*! This is a function that creates an instance of the operator, and is
	needed for the Iop::Description to work.
*/
static Iop* GinzburgDenoiseFilterPluginCreate(Node* node)
{
	return new GinzburgDenoiseFilterPlugin(node);
}

/*! The Iop::Description is how NUKE knows what the name of the operator is,
	how to create one, and the menu item to show the user. The menu item may be
	0 if you do not want the operator to be visible.
 */
const Iop::Description GinzburgDenoiseFilterPlugin::description(CLASS, "GinzburgDenoiseFilter/GinzburgDenoiseFilterPlugin",
								GinzburgDenoiseFilterPluginCreate);

const char* const GinzburgDenoiseFilterPlugin::CLASS = "GinzburgDenoiseFilterPlugin";
const char* const GinzburgDenoiseFilterPlugin::HELP = "GinzburgDenoiseFilterPlugin";

void GinzburgDenoiseFilterPlugin::_validate(bool for_real)
{
	copy_info(0); // copy bbox channels etc from input0, which will validate it.
	info_.channels();
	info_.pad( _size);
}

const OutputContext& GinzburgDenoiseFilterPlugin::inputContext(int i, int n, OutputContext& context) const
{
	context = outputContext();
	switch (n) {
		case 0:
			break;
		case 1:
			context.setFrame(context.frame() + 1);
			break;
		case 2:
			context.setFrame(context.frame() + 2);
			break;
		case 3:
			context.setFrame(context.frame() + 3);
			break;
		case 4:
			context.setFrame(context.frame() - 1);
			break;
		case 5:
			context.setFrame(context.frame() - 2);
			break;
		case 6:
			context.setFrame(context.frame() - 3);
			break;
	}
	return context;
}

void GinzburgDenoiseFilterPlugin::_request(int x, int y, int r, int t, ChannelMask channels, int count)
{
	// for this example, we're only interested in the RGB channels
	//input(0)->request( x, y, r, t, channels, count );
	ChannelSet c1(channels);
	c1 += (_mv[0]);
	c1 += (_mv[1]);
	c1 += (_beauty[0]);
	c1 += (_beauty[1]);
	c1 += (_beauty[2]);
	c1 += (_position[0]);
	c1 += (_position[1]);
	c1 += (_position[2]);
	c1 += (_depth[0]);
	c1 += (_normal[0]);
	c1 += (_normal[1]);
	c1 += (_normal[2]);
	c1 += (_albedo[0]);
	c1 += (_albedo[1]);
	c1 += (_albedo[2]);
	c1 += (_extraChannel[0][0]);
	c1 += (_extraChannel[0][1]);
	c1 += (_extraChannel[0][2]);
	c1 += (_extraChannel[1][0]);
	c1 += (_extraChannel[1][1]);
	c1 += (_extraChannel[1][2]);
	c1 += (_extraChannel[2][0]);
	c1 += (_extraChannel[2][1]);
	c1 += (_extraChannel[2][2]);
	c1 += (_extraChannel[3][0]);
	c1 += (_extraChannel[3][1]);
	c1 += (_extraChannel[3][2]);

	xMax = r;
	yMax = t;
	input(0) -> request(x- _size,y- _size,r+ _size,t+  _size,c1,count * 2);
	input(1) -> request(x- _size,y- _size,r+ _size,t+  _size,c1,count * 2);
	input(2) -> request(x- _size,y- _size,r+ _size,t+  _size,c1,count * 2);
	input(3) -> request(x- _size,y- _size,r+ _size,t+  _size,c1,count * 2);
	input(4) -> request(x- _size,y- _size,r+ _size,t+  _size,c1,count * 2);
	input(5) -> request(x- _size,y- _size,r+ _size,t+  _size,c1,count * 2);
	input(6) -> request(x- _size,y- _size,r+ _size,t+  _size,c1,count * 2);
}

/*! For each line in the area passed to request(), this will be called. It must
	calculate the image data for a region at vertical position y, and between
	horizontal positions x and r, and write it to the passed row
	structure. Usually this works by asking the input for data, and modifying
	it.
 */
void GinzburgDenoiseFilterPlugin::engine ( int y, int x, int r, ChannelMask channels, Row& outRow )
{
	ChannelMask rgbMask(Mask_RGB);
	ChannelSet c1(channels);
	c1 += (_mv[0]);
	c1 += (_mv[1]);
	c1 += (_mv[2]);
	c1 += (_beauty[0]);
	c1 += (_beauty[1]);
	c1 += (_beauty[2]);
	c1 += (_position[0]);
	c1 += (_position[1]);
	c1 += (_position[2]);
	c1 += (_depth[0]);
	c1 += (_normal[0]);
	c1 += (_normal[1]);
	c1 += (_normal[2]);
	c1 += (_albedo[0]);
	c1 += (_albedo[1]);
	c1 += (_albedo[2]);
	c1 += (_extraChannel[0][0]);
	c1 += (_extraChannel[0][1]);
	c1 += (_extraChannel[0][2]);
	c1 += (_extraChannel[1][0]);
	c1 += (_extraChannel[1][1]);
	c1 += (_extraChannel[1][2]);
	c1 += (_extraChannel[2][0]);
	c1 += (_extraChannel[2][1]);
	c1 += (_extraChannel[2][2]);
	c1 += (_extraChannel[3][0]);
	c1 += (_extraChannel[3][1]);
	c1 += (_extraChannel[3][2]);

	Tile tile(  input0(), x - _size, y - _size, r + _size, y + _size, c1);
	Tile tile1( input1(), x - _size , y - _size , r + _size, y + _size , c1);
	Tile tile2( *input(2), x - _size, y - _size, r + _size, y +  _size, c1);
	Tile tile3( *input(3), x - _size, y - _size, r + _size, y +  _size, c1);
	Tile tile4( *input(4), x - _size, y - _size, r + _size, y + _size, c1);
	Tile tile5( *input(5), x - _size, y - _size, r + _size, y +  _size, c1);
	Tile tile6( *input(6), x - _size, y - _size, r + _size, y +  _size, c1);

	foreach(z, c1) outRow.writable(z);
	if ( aborted() ) {
		std::cerr << "Aborted!";
		return;
	}
	float pDist[6];
	float pColor[6];
	float pZt[6];
	float pZtA[6];
	float pPos;
	float pChannel[6][15];
	float mvTrace[6][2];
	float temporalPointsXY[6][2];
	float maxDist[6];
	float sumWeightXY[6];
	float colorValue = 0;
	float spatTemporalWeight = 0;
	float depthValue = 0;

	float albedoValue0[] = {0.0f,0.0f,0.0f};
	float albedoValue1[] = {0.0f,0.0f,0.0f};

	float beautyValue0[] = {0.0f,0.0f,0.0f};
	float beautyValue1[] = {0.0f,0.0f,0.0f};

	float normalValue0[] = {0.0f,0.0f,0.0f};
	float normalValue1[] = {0.0f,0.0f,0.0f};

	float currWeightSpat = 0;
	float sumWeightSpat = 0;
	float beautyDist = 0;
	float albedoDist = 0;
	float depthDist = 0;
	float normalDist = 0;
	float positionDist = 0;

	float*  outr = outRow.writable(_beauty[0]);
	float*  outg = outRow.writable(_beauty[1]);
	float*  outb = outRow.writable(_beauty[2]);

	float*  outE0r = outRow.writable(_extraChannel[0][0]);
	float*  outE0g = outRow.writable(_extraChannel[0][1]);
	float*  outE0b = outRow.writable(_extraChannel[0][2]);

	float*  outE1r = outRow.writable(_extraChannel[1][0]);
	float*  outE1g = outRow.writable(_extraChannel[1][1]);
	float*  outE1b = outRow.writable(_extraChannel[1][2]);

	float*  outE2r = outRow.writable(_extraChannel[2][0]);
	float*  outE2g = outRow.writable(_extraChannel[2][1]);
	float*  outE2b = outRow.writable(_extraChannel[2][2]);

	float*  outE3r = outRow.writable(_extraChannel[3][0]);
	float*  outE3g = outRow.writable(_extraChannel[3][1]);
	float*  outE3b = outRow.writable(_extraChannel[3][2]);

	for(int i = x; i < r; i++){
		float resultValue[3];
		float resultValueE[4][3];
		float currentWeight = 0;
		float sumWeight = 1;
		float sumWeightSpat = 1;

		for(int k1 = 0; k1 < 6; k1++){
			temporalPointsXY[k1][0] = 0;
			temporalPointsXY[k1][1] = 0;
		}

		for(int k2 = 0; k2 < 6; k2++){
			sumWeightXY[k2] = 0;
		}

		maxDist[0] = 1000000;
		maxDist[1] = 1000000;
		maxDist[2] = 1000000;
		maxDist[3] = 1000000;
		maxDist[4] = 1000000;
		maxDist[5] = 1000000;

		resultValue[0] = tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)];
		resultValue[1] = tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)];
		resultValue[2] = tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)];

		resultValueE[0][0] = tile[_extraChannel[0][0]][tile.clampy(y)][tile.clampx(i)];
		resultValueE[0][1] = tile[_extraChannel[0][1]][tile.clampy(y)][tile.clampx(i)];
		resultValueE[0][2] = tile[_extraChannel[0][2]][tile.clampy(y)][tile.clampx(i)];

		resultValueE[1][0] = tile[_extraChannel[1][0]][tile.clampy(y)][tile.clampx(i)];
		resultValueE[1][1] = tile[_extraChannel[1][1]][tile.clampy(y)][tile.clampx(i)];
		resultValueE[1][2] = tile[_extraChannel[1][2]][tile.clampy(y)][tile.clampx(i)];

		resultValueE[2][0] = tile[_extraChannel[2][0]][tile.clampy(y)][tile.clampx(i)];
		resultValueE[2][1] = tile[_extraChannel[2][1]][tile.clampy(y)][tile.clampx(i)];
		resultValueE[2][2] = tile[_extraChannel[2][2]][tile.clampy(y)][tile.clampx(i)];

		resultValueE[3][0] = tile[_extraChannel[3][0]][tile.clampy(y)][tile.clampx(i)];
		resultValueE[3][1] = tile[_extraChannel[3][1]][tile.clampy(y)][tile.clampx(i)];
		resultValueE[3][2] = tile[_extraChannel[3][2]][tile.clampy(y)][tile.clampx(i)];

		albedoValue0[0] = tile[_albedo[0]][ tile.clampy(y)][ tile.clampx(i)];
		albedoValue0[1] = tile[_albedo[1]][ tile.clampy(y)][ tile.clampx(i)];
		albedoValue0[2] = tile[_albedo[2]][ tile.clampy(y)][ tile.clampx(i)];

		beautyValue0[0] = tile[_beauty[0]][ tile.clampy(y)][ tile.clampx(i)];
		beautyValue0[1] = tile[_beauty[1]][ tile.clampy(y)][ tile.clampx(i)];
		beautyValue0[2] = tile[_beauty[2]][ tile.clampy(y)][ tile.clampx(i)];
		
		normalValue0[0] = tile[_normal[0]][ tile.clampy(y)][ tile.clampx(i)];
		normalValue0[1] = tile[_normal[1]][ tile.clampy(y)][ tile.clampx(i)];
		normalValue0[2] = tile[_normal[2]][ tile.clampy(y)][ tile.clampx(i)];

		depthValue = tile[_depth[0]][ tile.clampy(y)][ tile.clampx(i)];


		for ( int px = -searchRadius; px < searchRadius+1; px++ )
			for ( int py = -searchRadius; py < searchRadius+1; py++ ) {

				for(int j = 0; j < 6; j++)
					for(int j1 = 0; j1 < 2; j1++)
						mvTrace[j][j1] = 0.0f;

				// MotionVector tracer
				if(useMV){
					mvTrace[0][0] = tile[_mv[0]][tile.clampy(y)][tile.clampx(i)]*MotionVectorMult;
					mvTrace[0][1] = tile[_mv[1]][tile.clampy(y)][tile.clampx(i)]*MotionVectorMult;

					mvTrace[1][0] = tile1[_mv[0]][tile1.clampy(y+mvTrace[0][1])][tile1.clampx(i+mvTrace[0][0])]*MotionVectorMult+mvTrace[0][0];
					mvTrace[1][1] = tile1[_mv[1]][tile1.clampy(y+mvTrace[0][1])][tile1.clampx(i+mvTrace[0][0])]*MotionVectorMult+mvTrace[0][1];

					mvTrace[2][0] = tile2[_mv[0]][tile2.clampy(y+mvTrace[1][1])][tile2.clampx(i+mvTrace[1][0])]*MotionVectorMult+mvTrace[1][0];
					mvTrace[2][1] = tile2[_mv[1]][tile2.clampy(y+mvTrace[1][1])][tile2.clampx(i+mvTrace[1][0])]*MotionVectorMult+mvTrace[1][1];

					mvTrace[3][0] = -tile4[_mv[0]][tile4.clampy(y+py)][tile4.clampx(i+px)]*MotionVectorMult;
					mvTrace[3][1] = -tile4[_mv[1]][tile4.clampy(y+py)][tile4.clampx(i+px)]*MotionVectorMult;

					mvTrace[4][0] = -tile5[_mv[0]][tile5.clampy(y+mvTrace[3][1]+py)][tile5.clampx(i+mvTrace[3][0]+px)]*MotionVectorMult+mvTrace[3][0];
					mvTrace[4][1] = -tile5[_mv[1]][tile5.clampy(y+mvTrace[3][1]+py)][tile5.clampx(i+mvTrace[3][0]+px)]*MotionVectorMult+mvTrace[3][1];

					mvTrace[5][0] = -tile6[_mv[0]][tile6.clampy(y+mvTrace[4][1]+py)][tile6.clampx(i+mvTrace[4][0]+px)]*MotionVectorMult+mvTrace[4][0];
					mvTrace[5][1] = -tile6[_mv[1]][tile6.clampy(y+mvTrace[4][1]+py)][tile6.clampx(i+mvTrace[4][0]+px)]*MotionVectorMult+mvTrace[4][1];
				}

				// Distance Treshold
				pDist[0] = sqrt((float)_epsX * (tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile1[_position[0]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])*
								(tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile1[_position[0]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])+
							_epsY * (tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile1[_position[1]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])*
								(tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile1[_position[1]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])+
							_epsZ * (tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile1[_position[2]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])*
								(tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile1[_position[2]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)]));

				pDist[1] = sqrt((float)_epsX * (tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile2[_position[0]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])*
								(tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile2[_position[0]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])+
							_epsY * (tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile2[_position[1]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])*
								(tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile2[_position[1]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])+
							_epsZ * (tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile2[_position[2]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])*
								(tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile2[_position[2]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)]));

				pDist[2] = sqrt((float)_epsX * (tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile3[_position[0]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])*
								(tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile3[_position[0]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])+
							_epsY * (tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile3[_position[1]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])*
								(tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile3[_position[1]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])+
							_epsZ * (tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile3[_position[2]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])*
								(tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile3[_position[2]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)]));

				pDist[3] = sqrt((float)_epsX * (tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile4[_position[0]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])*
								(tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile4[_position[0]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])+
							_epsY * (tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile4[_position[1]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])*
								(tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile4[_position[1]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])+
							_epsZ * (tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile4[_position[2]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])*
								(tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile4[_position[2]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)]));

				pDist[4] = sqrt((float)_epsX * (tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile5[_position[0]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])*
								(tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile5[_position[0]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])+
							_epsY * (tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile5[_position[1]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])*
								(tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile5[_position[1]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])+
							_epsZ * (tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile5[_position[2]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])*
								(tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile5[_position[2]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)]));

				pDist[5] = sqrt((float)_epsX * (tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile6[_position[0]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])*
								(tile[_position[0]][tile.clampy(y)][tile.clampx(i)] - tile6[_position[0]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])+
							_epsY * (tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile6[_position[1]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])*
								(tile[_position[1]][tile.clampy(y)][tile.clampx(i)] - tile6[_position[1]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])+
							_epsZ * (tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile6[_position[2]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])*
								(tile[_position[2]][tile.clampy(y)][tile.clampx(i)] - tile6[_position[2]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)]));

				// color treshold
				pColor[0] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[0]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])*
							(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[0]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])+
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[1]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])*
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[1]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])+
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[2]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])*
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[2]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)]));

				pColor[1] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[0]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])*
							(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[0]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])+
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[1]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])*
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[1]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])+
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[2]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])*
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[2]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)]));

				pColor[2] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[0]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])*
							(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[0]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])+
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[1]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])*
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[1]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])+
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[2]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])*
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[2]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)]));

				pColor[3] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[0]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])*
							(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[0]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])+
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[1]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])*
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[1]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])+
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[2]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])*
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[2]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)]));

				pColor[4] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[0]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])*
							(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[0]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])+
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[1]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])*
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[1]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])+
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[2]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])*
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[2]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)]));

				pColor[5] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[0]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])*
							(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[0]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])+
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[1]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])*
							(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[1]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])+
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[2]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])*
							(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[2]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)]));

				pZtA[0] = sqrt((float)(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[0]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])*
							(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[0]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])+
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[1]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])*
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[1]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])+
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[2]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)])*
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[2]][tile1.clampy(y+mvTrace[0][1]+py)][tile1.clampx(i+mvTrace[0][0]+px)]));

				pZtA[1] = sqrt((float)(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[0]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])*
							(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[0]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])+
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[1]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])*
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[1]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])+
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[2]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)])*
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[2]][tile2.clampy(y+mvTrace[1][1]+py)][tile2.clampx(i+mvTrace[1][0]+px)]));

				pZtA[2] = sqrt((float)(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[0]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])*
							(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[0]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])+
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[1]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])*
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[1]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])+
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[2]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)])*
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[2]][tile3.clampy(y+mvTrace[2][1]+py)][tile3.clampx(i+mvTrace[2][0]+px)]));

				pZtA[3] = sqrt((float)(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[0]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])*
							(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[0]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])+
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[1]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])*
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[1]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])+
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[2]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)])*
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[2]][tile4.clampy(y+mvTrace[3][1]+py)][tile4.clampx(i+mvTrace[3][0]+px)]));

				pZtA[4] = sqrt((float)(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile5[_albedo[0]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])*
							(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile5[_albedo[0]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])+
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile5[_albedo[1]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])*
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile5[_albedo[1]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])+
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile5[_albedo[2]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)])*
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile5[_albedo[2]][tile5.clampy(y+mvTrace[4][1]+py)][tile5.clampx(i+mvTrace[4][0]+px)]));

				pZtA[5] = sqrt((float)(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile6[_albedo[0]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])*
							(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile6[_albedo[0]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])+
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile6[_albedo[1]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])*
							(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile6[_albedo[1]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])+
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile6[_albedo[2]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)])*
							(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile6[_albedo[2]][tile6.clampy(y+mvTrace[5][1]+py)][tile6.clampx(i+mvTrace[5][0]+px)]));

				for (int frame = 0; frame < nFrames-1; frame++)
					if(	(pDist[frame] <= _eps)&&
						(pColor[frame] <= _epsColor)&&
						(pZtA[frame] <= _wAt)&&
						(pDist[frame] < maxDist[frame])&&
						((i+mvTrace[frame][0]+px)< xMax)&&
						((i+mvTrace[frame][0]+px)> 0)&&
						((y+mvTrace[frame][1]+py)< yMax)&&
						((y+mvTrace[frame][1]+py)> 0)&&
						(_lic)){
							maxDist[frame] = pDist[frame]; 
							temporalPointsXY[frame][0] = i+mvTrace[frame][0]+px;
							temporalPointsXY[frame][1] = y+mvTrace[frame][1]+py;
							sumWeightXY[frame] = 1;
						}
			}

			// Current channel
			for ( int px = -kernelRadius; px < kernelRadius+1; px++ )
				for ( int py = -kernelRadius; py < kernelRadius+1; py++ ){
					pChannel[0][0] = tile1[_beauty[0]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][1] = tile1[_beauty[1]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][2] = tile1[_beauty[2]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][3] = tile1[_extraChannel[0][0]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][4] = tile1[_extraChannel[0][1]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][5] = tile1[_extraChannel[0][2]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][6] = tile1[_extraChannel[1][0]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][7] = tile1[_extraChannel[1][1]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][8] = tile1[_extraChannel[1][2]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][9] = tile1[_extraChannel[2][0]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][10] = tile1[_extraChannel[2][1]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][11] = tile1[_extraChannel[2][2]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][12] = tile1[_extraChannel[3][0]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][13] = tile1[_extraChannel[3][1]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];
					pChannel[0][14] = tile1[_extraChannel[3][2]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)];

					pChannel[1][0] = tile2[_beauty[0]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][1] = tile2[_beauty[1]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][2] = tile2[_beauty[2]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][3] = tile2[_extraChannel[0][0]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][4] = tile2[_extraChannel[0][1]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][5] = tile2[_extraChannel[0][2]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][6] = tile2[_extraChannel[1][0]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][7] = tile2[_extraChannel[1][1]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][8] = tile2[_extraChannel[1][2]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][9] = tile2[_extraChannel[2][0]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][10] = tile2[_extraChannel[2][1]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][11] = tile2[_extraChannel[2][2]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][12] = tile2[_extraChannel[3][0]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][13] = tile2[_extraChannel[3][1]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];
					pChannel[1][14] = tile2[_extraChannel[3][2]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)];

					pChannel[2][0] = tile3[_beauty[0]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][1] = tile3[_beauty[1]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][2] = tile3[_beauty[2]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][3] = tile3[_extraChannel[0][0]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][4] = tile3[_extraChannel[0][1]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][5] = tile3[_extraChannel[0][2]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][6] = tile3[_extraChannel[1][0]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][7] = tile3[_extraChannel[1][1]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][8] = tile3[_extraChannel[1][2]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][9] = tile3[_extraChannel[2][0]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][10] = tile3[_extraChannel[2][1]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][11] = tile3[_extraChannel[2][2]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][12] = tile3[_extraChannel[3][0]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][13] = tile3[_extraChannel[3][1]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];
					pChannel[2][14] = tile3[_extraChannel[3][2]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)];

					pChannel[3][0] = tile4[_beauty[0]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][1] = tile4[_beauty[1]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][2] = tile4[_beauty[2]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][3] = tile4[_extraChannel[0][0]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][4] = tile4[_extraChannel[0][1]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][5] = tile4[_extraChannel[0][2]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][6] = tile4[_extraChannel[1][0]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][7] = tile4[_extraChannel[1][1]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][8] = tile4[_extraChannel[1][2]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][9] = tile4[_extraChannel[2][0]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][10] = tile4[_extraChannel[2][1]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][11] = tile4[_extraChannel[2][2]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][12] = tile4[_extraChannel[3][0]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][13] = tile4[_extraChannel[3][1]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];
					pChannel[3][14] = tile4[_extraChannel[3][2]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)];

					pChannel[4][0] = tile5[_beauty[0]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][1] = tile5[_beauty[1]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][2] = tile5[_beauty[2]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][3] = tile5[_extraChannel[0][0]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][4] = tile5[_extraChannel[0][1]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][5] = tile5[_extraChannel[0][2]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][6] = tile5[_extraChannel[1][0]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][7] = tile5[_extraChannel[1][1]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][8] = tile5[_extraChannel[1][2]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][9] = tile5[_extraChannel[2][0]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][10] = tile5[_extraChannel[2][1]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][11] = tile5[_extraChannel[2][2]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][12] = tile5[_extraChannel[3][0]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][13] = tile5[_extraChannel[3][1]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];
					pChannel[4][14] = tile5[_extraChannel[3][2]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)];

					pChannel[5][0] = tile6[_beauty[0]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][1] = tile6[_beauty[1]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][2] = tile6[_beauty[2]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][3] = tile6[_extraChannel[0][0]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][4] = tile6[_extraChannel[0][1]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][5] = tile6[_extraChannel[0][2]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][6] = tile6[_extraChannel[1][0]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][7] = tile6[_extraChannel[1][1]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][8] = tile6[_extraChannel[1][2]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][9] = tile6[_extraChannel[2][0]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][10] = tile6[_extraChannel[2][1]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][11] = tile6[_extraChannel[2][2]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][12] = tile6[_extraChannel[3][0]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][13] = tile6[_extraChannel[3][1]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];
					pChannel[5][14] = tile6[_extraChannel[3][2]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)];

					pZt[0] =  abs(tile[_depth[0]][tile.clampy(y)][tile.clampx(i)] - tile1[_depth[0]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)]);
					pZt[1] =  abs(tile[_depth[0]][tile.clampy(y)][tile.clampx(i)] - tile2[_depth[0]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)]);
					pZt[2] =  abs(tile[_depth[0]][tile.clampy(y)][tile.clampx(i)] - tile3[_depth[0]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)]);
					pZt[3] =  abs(tile[_depth[0]][tile.clampy(y)][tile.clampx(i)] - tile4[_depth[0]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)]);
					pZt[4] =  abs(tile[_depth[0]][tile.clampy(y)][tile.clampx(i)] - tile5[_depth[0]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)]);
					pZt[5] =  abs(tile[_depth[0]][tile.clampy(y)][tile.clampx(i)] - tile6[_depth[0]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)]);

					pZtA[0] = sqrt((float)(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[0]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)])*
								(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[0]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)])+
								(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[1]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)])*
								(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[1]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)])+
								(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[2]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)])*
								(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile1[_albedo[2]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)]));
					pZtA[1] = sqrt((float)(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[0]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)])*
								(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[0]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)])+
								(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[1]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)])*
								(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[1]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)])+
								(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[2]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)])*
								(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile2[_albedo[2]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)]));
					pZtA[2] = sqrt((float)(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[0]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)])*
								(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[0]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)])+
								(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[1]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)])*
								(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[1]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)])+
								(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[2]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)])*
								(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile3[_albedo[2]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)]));
					pZtA[3] = sqrt((float)(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[0]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)])*
								(tile[_albedo[0]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[0]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)])+
								(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[1]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)])*
								(tile[_albedo[1]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[1]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)])+
								(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[2]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)])*
								(tile[_albedo[2]][tile.clampy(y)][tile.clampx(i)] - tile4[_albedo[2]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)]));
					pZtA[4] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[0]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)])*
								(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[0]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)])+
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[1]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)])*
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[1]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)])+
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[2]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)])*
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[2]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)]));
					pZtA[5] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[0]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)])*
								(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[0]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)])+
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[1]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)])*
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[1]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)])+
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[2]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)])*
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[2]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)]));

					pColor[0] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[0]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)])*
								(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[0]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)])+
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[1]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)])*
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[1]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)])+
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[2]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)])*
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile1[_beauty[2]][tile1.clampy(temporalPointsXY[0][1]+py)][tile1.clampx(temporalPointsXY[0][0]+px)]));
					pColor[1] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[0]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)])*
								(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[0]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)])+
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[1]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)])*
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[1]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)])+
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[2]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)])*
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile2[_beauty[2]][tile2.clampy(temporalPointsXY[1][1]+py)][tile2.clampx(temporalPointsXY[1][0]+px)]));
					pColor[2] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[0]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)])*
								(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[0]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)])+
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[1]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)])*
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[1]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)])+
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[2]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)])*
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile3[_beauty[2]][tile3.clampy(temporalPointsXY[2][1]+py)][tile3.clampx(temporalPointsXY[2][0]+px)]));
					pColor[3] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[0]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)])*
								(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[0]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)])+
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[1]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)])*
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[1]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)])+
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[2]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)])*
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile4[_beauty[2]][tile4.clampy(temporalPointsXY[3][1]+py)][tile4.clampx(temporalPointsXY[3][0]+px)]));
					pColor[4] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[0]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)])*
								(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[0]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)])+
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[1]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)])*
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[1]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)])+
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[2]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)])*
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile5[_beauty[2]][tile5.clampy(temporalPointsXY[4][1]+py)][tile5.clampx(temporalPointsXY[4][0]+px)]));
					pColor[5] = sqrt((float)(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[0]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)])*
								(tile[_beauty[0]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[0]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)])+
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[1]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)])*
								(tile[_beauty[1]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[1]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)])+
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[2]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)])*
								(tile[_beauty[2]][tile.clampy(y)][tile.clampx(i)] - tile6[_beauty[2]][tile6.clampy(temporalPointsXY[5][1]+py)][tile6.clampx(temporalPointsXY[5][0]+px)]));

					albedoValue1[0] = tile[_albedo[0]][ tile.clampy(y + py)][ tile.clampx(i + px)];
					albedoValue1[1] = tile[_albedo[1]][ tile.clampy(y + py)][ tile.clampx(i + px)];
					albedoValue1[2] = tile[_albedo[2]][ tile.clampy(y + py)][ tile.clampx(i + px)];

					beautyValue1[0] = tile[_beauty[0]][ tile.clampy(y + py)][ tile.clampx(i + px)];
					beautyValue1[1] = tile[_beauty[1]][ tile.clampy(y + py)][ tile.clampx(i + px)];
					beautyValue1[2] = tile[_beauty[2]][ tile.clampy(y + py)][ tile.clampx(i + px)];

					normalValue1[0] = tile[_normal[0]][ tile.clampy(y + py)][ tile.clampx(i + px)];
					normalValue1[1] = tile[_normal[1]][ tile.clampy(y + py)][ tile.clampx(i + px)];
					normalValue1[2] = tile[_normal[2]][ tile.clampy(y + py)][ tile.clampx(i + px)];

					beautyDist = sqrt((float)(beautyValue0[0]-beautyValue1[0])*(beautyValue0[0]-beautyValue1[0])+
								(beautyValue0[1]-beautyValue1[1])*(beautyValue0[1]-beautyValue1[1])+
								(beautyValue0[2]-beautyValue1[2])*(beautyValue0[2]-beautyValue1[2]));

					albedoDist = sqrt((float)(albedoValue0[0]-albedoValue1[0])*(albedoValue0[0]-albedoValue1[0])+
								(albedoValue0[1]-albedoValue1[1])*(albedoValue0[1]-albedoValue1[1])+
								(albedoValue0[2]-albedoValue1[2])*(albedoValue0[2]-albedoValue1[2]));

					depthDist = abs(depthValue - tile[_depth[0]][ tile.clampy(y + py)][ tile.clampx(i + px) ]);

					normalDist = sqrt((float)(normalValue0[0]-normalValue1[0])*(normalValue0[0]-normalValue1[0])+ 
								(normalValue0[1]-normalValue1[1])*(normalValue0[1]-normalValue1[1])+
								(normalValue0[2]-normalValue1[2])*(normalValue0[2]-normalValue1[2]));

					positionDist = sqrt((float)px*px+py*py);

					spatTemporalWeight = 0;

					for(int k = 0; k < nFrames-1; k++){
						spatTemporalWeight += sumWeightXY[k]/(nFrames-1);
					}

				for(int k = 0; k < nFrames-1; k++){	
					pPos = sqrt((float)(px*px+py*py));
					currentWeight = sumWeightXY[k]*_wT/(exp((pZt[k]/_wDist)*(pZt[k]/_wDist)*0.5)*
									exp((pColor[k]/_wColor)*(pColor[k]/_wColor)*0.5)*
									exp((pZtA[k]/_wAt)*(pZtA[k]/_wAt)*0.5)*
									exp((pPos/_wPosition)*(pPos/_wPosition)*0.5));

					// currentWeight *= sumWeightXY[k];
					resultValue[0] += pChannel[k][0]*currentWeight;
					resultValue[1] += pChannel[k][1]*currentWeight;
					resultValue[2] += pChannel[k][2]*currentWeight;

					resultValueE[0][0] += pChannel[k][3]*currentWeight;
					resultValueE[0][1] += pChannel[k][4]*currentWeight;
					resultValueE[0][2] += pChannel[k][5]*currentWeight;

					resultValueE[1][0] += pChannel[k][6]*currentWeight;
					resultValueE[1][1] += pChannel[k][7]*currentWeight;
					resultValueE[1][2] += pChannel[k][8]*currentWeight;

					resultValueE[2][0] += pChannel[k][9]*currentWeight;
					resultValueE[2][1] += pChannel[k][10]*currentWeight;
					resultValueE[2][2] += pChannel[k][11]*currentWeight;

					resultValueE[3][0] += pChannel[k][12]*currentWeight;
					resultValueE[3][1] += pChannel[k][13]*currentWeight;
					resultValueE[3][2] += pChannel[k][14]*currentWeight;

					sumWeight += currentWeight;
				}

				currWeightSpat = ( 1 - spatTemporalWeight)*_wS/(exp((normalDist/_wN)*(normalDist/_wN)*0.5)*
										exp((beautyDist/_wColor)*(beautyDist/_wB)*0.5)*
										exp((positionDist/_wPosition)*(positionDist/_wP)*0.5)*
										exp((depthDist/_wD)*(depthDist/_wD)*0.5)*
										exp((albedoDist/_wA)*(albedoDist/_wA)*0.5));

				resultValue[0] += tile[_beauty[0]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;
				resultValue[1] += tile[_beauty[1]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;
				resultValue[2] += tile[_beauty[2]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;

				resultValueE[0][0] += tile[_extraChannel[0][0]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;
				resultValueE[0][1] += tile[_extraChannel[0][1]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;
				resultValueE[0][2] += tile[_extraChannel[0][2]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;

				resultValueE[1][0] += tile[_extraChannel[1][0]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;
				resultValueE[1][1] += tile[_extraChannel[1][1]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;
				resultValueE[1][2] += tile[_extraChannel[1][2]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;

				resultValueE[2][0] += tile[_extraChannel[2][0]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;
				resultValueE[2][1] += tile[_extraChannel[2][1]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;
				resultValueE[2][2] += tile[_extraChannel[2][2]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;

				resultValueE[3][0] += tile[_extraChannel[3][0]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;
				resultValueE[3][1] += tile[_extraChannel[3][1]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;
				resultValueE[3][2] += tile[_extraChannel[3][2]][tile.clampy(y+py)][tile.clampx(i+px)]*currWeightSpat;

				sumWeight += currWeightSpat;
			}
				outr[i] = resultValue[0]/sumWeight;
				outg[i] = resultValue[1]/sumWeight;
				outb[i] = resultValue[2]/sumWeight;

				outE0r[i] = resultValueE[0][0]/sumWeight;
				outE0g[i] = resultValueE[0][1]/sumWeight;
				outE0b[i] = resultValueE[0][2]/sumWeight;

				outE1r[i] = resultValueE[1][0]/sumWeight;
				outE1g[i] = resultValueE[1][1]/sumWeight;
				outE1b[i] = resultValueE[1][2]/sumWeight;

				outE2r[i] = resultValueE[2][0]/sumWeight;
				outE2g[i] = resultValueE[2][1]/sumWeight;
				outE2b[i] = resultValueE[2][2]/sumWeight;

				outE3r[i] = resultValueE[3][0]/sumWeight;
				outE3g[i] = resultValueE[3][1]/sumWeight;
				outE3b[i] = resultValueE[3][2]/sumWeight;
	}
	
}
