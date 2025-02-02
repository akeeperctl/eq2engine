//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2022
//////////////////////////////////////////////////////////////////////////////////
// Description: OpenGL Worker
//////////////////////////////////////////////////////////////////////////////////

#pragma once

static constexpr const int WORK_NOT_STARTED = -20000;
static constexpr const int WORK_TAKEN_SLOT = -10000;
static constexpr const int WORK_PENDING		= 10000;
static constexpr const int WORK_EXECUTING	= 20000;

class GLLibraryWorkerHandler
{
public:
	virtual void	BeginAsyncOperation(uintptr_t threadId) = 0;
	virtual void	EndAsyncOperation() = 0;
	virtual bool	IsMainThread(uintptr_t threadId) const = 0;
};

class GLWorkerThread : public Threading::CEqThread
{
	friend class ShaderAPIGL;

public:
	using FUNC_TYPE = EqFunction<int()>;

	GLWorkerThread() = default;

	void	Init(GLLibraryWorkerHandler* workHandler);
	void	Shutdown();

	// syncronous execution
	int		WaitForExecute(const char* name, FUNC_TYPE f);

	// asyncronous execution
	void	Execute(const char* name, FUNC_TYPE f);

protected:
	struct Work
	{
		FUNC_TYPE				func;
		int						result{ WORK_NOT_STARTED };
		bool					sync{ false };
	};

	int		Run() override;

	FixedArray<Work, 32>					m_workRingPool;
	FixedArray<Threading::CEqSignal, 32>	m_completionSignal;
	GLLibraryWorkerHandler*					m_workHandler{ nullptr };
};

extern GLWorkerThread g_glWorker;
