//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Pacifier
//////////////////////////////////////////////////////////////////////////////////

#pragma once

// Use these to display a pacifier like:
// <pPrefix>: 0...1...2...3...4...5...6...7...8...9... (time)
void StartPacifier( char const *pPrefix );
void UpdatePacifier( float flPercent );	// percent value between 0 and 1.
void EndPacifier( bool bCarriageReturn = true );