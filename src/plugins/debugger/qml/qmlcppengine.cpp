#include "qmlcppengine.h"
#include "debuggerstartparameters.h"
#include "qmlengine.h"
#include "debuggermainwindow.h"
#include "debuggercore.h"

#include <qmljseditor/qmljseditorconstants.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>

#include <QtCore/QTimer>

namespace Debugger {
namespace Internal {

const int ConnectionWaitTimeMs = 5000;

DebuggerEngine *createCdbEngine(const DebuggerStartParameters &,
    DebuggerEngine *masterEngine, QString *);
DebuggerEngine *createGdbEngine(const DebuggerStartParameters &,
    DebuggerEngine *masterEngine);
DebuggerEngine *createQmlEngine(const DebuggerStartParameters &,
    DebuggerEngine *masterEngine);

DebuggerEngine *createQmlCppEngine(const DebuggerStartParameters &sp)
{
    QmlCppEngine *newEngine = new QmlCppEngine(sp);
    if (newEngine->cppEngine())
        return newEngine;
    delete newEngine;
    return 0;
}

class QmlCppEnginePrivate
{
public:
    QmlCppEnginePrivate();
    ~QmlCppEnginePrivate() {}

    friend class QmlCppEngine;
private:
    DebuggerEngine *m_qmlEngine;
    DebuggerEngine *m_cppEngine;
    DebuggerEngine *m_activeEngine;
    DebuggerState m_errorState;
};

QmlCppEnginePrivate::QmlCppEnginePrivate()
  : m_qmlEngine(0),
    m_cppEngine(0),
    m_activeEngine(0),
    m_errorState(InferiorRunOk)
{}


QmlCppEngine::QmlCppEngine(const DebuggerStartParameters &sp)
    : DebuggerEngine(sp), d(new QmlCppEnginePrivate)
{
    d->m_qmlEngine = createQmlEngine(sp, this);

    if (startParameters().cppEngineType == GdbEngineType) {
        d->m_cppEngine = createGdbEngine(sp, this);
    } else {
        QString errorMessage;
        d->m_cppEngine = Debugger::Internal::createCdbEngine(sp, this, &errorMessage);
        if (!d->m_cppEngine) {
            qWarning("%s", qPrintable(errorMessage));
            return;
        }
    }

    d->m_activeEngine = d->m_cppEngine;
    connect(d->m_cppEngine, SIGNAL(stateChanged(DebuggerState)),
        SLOT(slaveEngineStateChanged(DebuggerState)));
    connect(d->m_qmlEngine, SIGNAL(stateChanged(DebuggerState)),
        SLOT(slaveEngineStateChanged(DebuggerState)));
}

QmlCppEngine::~QmlCppEngine()
{
    delete d->m_qmlEngine;
    delete d->m_cppEngine;
}

void QmlCppEngine::setToolTipExpression(const QPoint & mousePos,
        TextEditor::ITextEditor *editor, int cursorPos)
{
    d->m_activeEngine->setToolTipExpression(mousePos, editor, cursorPos);
}

void QmlCppEngine::updateWatchData(const WatchData &data,
    const WatchUpdateFlags &flags)
{
    d->m_activeEngine->updateWatchData(data, flags);
}

void QmlCppEngine::watchPoint(const QPoint &point)
{
    d->m_cppEngine->watchPoint(point);
}

void QmlCppEngine::fetchMemory(MemoryAgent *ma, QObject *obj,
        quint64 addr, quint64 length)
{
    d->m_cppEngine->fetchMemory(ma, obj, addr, length);
}

void QmlCppEngine::fetchDisassembler(DisassemblerAgent *da)
{
    d->m_cppEngine->fetchDisassembler(da);
}

void QmlCppEngine::activateFrame(int index)
{
    d->m_cppEngine->activateFrame(index);
}

void QmlCppEngine::reloadModules()
{
    d->m_cppEngine->reloadModules();
}

void QmlCppEngine::examineModules()
{
    d->m_cppEngine->examineModules();
}

void QmlCppEngine::loadSymbols(const QString &moduleName)
{
    d->m_cppEngine->loadSymbols(moduleName);
}

void QmlCppEngine::loadAllSymbols()
{
    d->m_cppEngine->loadAllSymbols();
}

void QmlCppEngine::requestModuleSymbols(const QString &moduleName)
{
    d->m_cppEngine->requestModuleSymbols(moduleName);
}

void QmlCppEngine::reloadRegisters()
{
    d->m_cppEngine->reloadRegisters();
}

void QmlCppEngine::reloadSourceFiles()
{
    d->m_cppEngine->reloadSourceFiles();
}

void QmlCppEngine::reloadFullStack()
{
    d->m_cppEngine->reloadFullStack();
}

void QmlCppEngine::setRegisterValue(int regnr, const QString &value)
{
    d->m_cppEngine->setRegisterValue(regnr, value);
}

unsigned QmlCppEngine::debuggerCapabilities() const
{
    // ### this could also be an OR of both engines' capabilities
    return d->m_cppEngine->debuggerCapabilities();
}

bool QmlCppEngine::isSynchronous() const
{
    return d->m_activeEngine->isSynchronous();
}

QByteArray QmlCppEngine::qtNamespace() const
{
    return d->m_cppEngine->qtNamespace();
}

void QmlCppEngine::createSnapshot()
{
    d->m_cppEngine->createSnapshot();
}

void QmlCppEngine::updateAll()
{
    d->m_activeEngine->updateAll();
}

void QmlCppEngine::attemptBreakpointSynchronization()
{
    d->m_cppEngine->attemptBreakpointSynchronization();
    d->m_qmlEngine->attemptBreakpointSynchronization();
}

bool QmlCppEngine::acceptsBreakpoint(BreakpointId id) const
{
    return d->m_cppEngine->acceptsBreakpoint(id)
        || d->m_qmlEngine->acceptsBreakpoint(id);
}

void QmlCppEngine::selectThread(int index)
{
    d->m_cppEngine->selectThread(index);
}

void QmlCppEngine::assignValueInDebugger(const WatchData *data,
    const QString &expr, const QVariant &value)
{
    d->m_activeEngine->assignValueInDebugger(data, expr, value);
}

void QmlCppEngine::detachDebugger()
{
    d->m_qmlEngine->detachDebugger();
    d->m_cppEngine->detachDebugger();
}

void QmlCppEngine::executeStep()
{
    d->m_activeEngine->executeStep();
}

void QmlCppEngine::executeStepOut()
{
    d->m_activeEngine->executeStepOut();
}

void QmlCppEngine::executeNext()
{
    d->m_activeEngine->executeNext();
}

void QmlCppEngine::executeStepI()
{
    d->m_activeEngine->executeStepI();
}

void QmlCppEngine::executeNextI()
{
    d->m_activeEngine->executeNextI();
}

void QmlCppEngine::executeReturn()
{
    d->m_activeEngine->executeReturn();
}

void QmlCppEngine::continueInferior()
{
    if (d->m_activeEngine->state() == InferiorStopOk) {
        d->m_activeEngine->continueInferior();
    } else {
        notifyInferiorRunRequested();
    }
}

void QmlCppEngine::interruptInferior()
{
    if (d->m_activeEngine->state() == InferiorRunOk) {
        d->m_activeEngine->requestInterruptInferior();
    } else {
        if (d->m_activeEngine->state() == InferiorStopOk && (!checkErrorState(InferiorStopFailed))) {
            notifyInferiorStopOk();
        }
    }
}

void QmlCppEngine::requestInterruptInferior()
{
    DebuggerEngine::requestInterruptInferior();

    if (d->m_activeEngine->state() == InferiorRunOk) {
        d->m_activeEngine->requestInterruptInferior();
    }
}

void QmlCppEngine::executeRunToLine(const QString &fileName, int lineNumber)
{
    d->m_activeEngine->executeRunToLine(fileName, lineNumber);
}

void QmlCppEngine::executeRunToFunction(const QString &functionName)
{
    d->m_activeEngine->executeRunToFunction(functionName);
}

void QmlCppEngine::executeJumpToLine(const QString &fileName, int lineNumber)
{
    d->m_activeEngine->executeJumpToLine(fileName, lineNumber);
}

void QmlCppEngine::executeDebuggerCommand(const QString &command)
{
    d->m_activeEngine->executeDebuggerCommand(command);
}

void QmlCppEngine::frameUp()
{
    d->m_activeEngine->frameUp();
}

void QmlCppEngine::frameDown()
{
    d->m_activeEngine->frameDown();
}

/////////////////////////////////////////////////////////

bool QmlCppEngine::checkErrorState(const DebuggerState stateToCheck)
{
    if (d->m_errorState != stateToCheck)
        return false;

    // reset state ( so that more than one error can accumulate over time )
    d->m_errorState = InferiorRunOk;
    switch (stateToCheck) {
    case InferiorRunOk:
        // nothing to do
        break;
    case EngineRunFailed:
        notifyEngineRunFailed();
        break;
    case EngineSetupFailed:
        notifyEngineSetupFailed();
        break;
    case EngineShutdownFailed:
        notifyEngineShutdownFailed();
        break;
    case InferiorSetupFailed:
        notifyInferiorSetupFailed();
        break;
    case InferiorRunFailed:
        notifyInferiorRunFailed();
        break;
    case InferiorUnrunnable:
        notifyInferiorUnrunnable();
        break;
    case InferiorStopFailed:
        notifyInferiorStopFailed();
        break;
    case InferiorShutdownFailed:
        notifyInferiorShutdownFailed();
        break;
    default:
        // unexpected
        break;
    }
    return true;
}

void QmlCppEngine::notifyInferiorRunOk()
{
    DebuggerEngine::notifyInferiorRunOk();
}

void QmlCppEngine::setupEngine()
{
    d->m_cppEngine->startDebugger(runControl());
}

void QmlCppEngine::setupInferior()
{
    if (!checkErrorState(InferiorSetupFailed)) {
        notifyInferiorSetupOk();
    }
}

void QmlCppEngine::runEngine()
{
    if (!checkErrorState(EngineRunFailed)) {
        if (d->m_errorState == InferiorRunOk) {
            switch (d->m_activeEngine->state()) {
            case InferiorRunOk:
                notifyEngineRunAndInferiorRunOk();
                break;
            case InferiorStopOk:
                notifyEngineRunAndInferiorStopOk();
                break;
            default: // not supported?
                notifyEngineRunFailed();
                break;
            }
        } else {
            notifyEngineRunFailed();
        }
    }
}

void QmlCppEngine::shutdownInferior()
{
    if (!checkErrorState(InferiorShutdownFailed)) {
        if (d->m_cppEngine->state() == InferiorStopOk) {
            d->m_cppEngine->quitDebugger();
        } else {
            notifyInferiorShutdownOk();
        }
    }
}

void QmlCppEngine::initEngineShutdown()
{
    if (d->m_qmlEngine->state() != DebuggerFinished) {
        d->m_qmlEngine->quitDebugger();
    } else
    if (d->m_cppEngine->state() != DebuggerFinished) {
        d->m_cppEngine->quitDebugger();
    } else
    if (state() == EngineSetupRequested) {
        if (!runControl() || d->m_errorState == EngineSetupFailed) {
            notifyEngineSetupFailed();
        } else {
            notifyEngineSetupOk();
        }
    } else
    if (state() == InferiorStopRequested) {
        checkErrorState(InferiorStopFailed);
    } else
    if (state() == InferiorShutdownRequested && !checkErrorState(InferiorShutdownFailed)) {
        notifyInferiorShutdownOk();
    } else
    if (state() != DebuggerFinished) {
        quitDebugger();
    }
}

void QmlCppEngine::shutdownEngine()
{
    if (!checkErrorState(EngineShutdownFailed)) {
        showStatusMessage(tr("Debugging finished"));
        notifyEngineShutdownOk();
    }
}

void QmlCppEngine::setupSlaveEngine()
{
    if (d->m_qmlEngine->state() == DebuggerNotReady)
        d->m_qmlEngine->startDebugger(runControl());
}

void QmlCppEngine::slaveEngineStateChanged(const DebuggerState newState)
{
    DebuggerEngine *slaveEngine = qobject_cast<DebuggerEngine *>(sender());
    if (newState == InferiorStopOk && slaveEngine != d->m_activeEngine) {
        QString engineName = slaveEngine == d->m_cppEngine
            ? QLatin1String("C++") : QLatin1String("QML");
        showStatusMessage(tr("%1 debugger activated").arg(engineName));
        d->m_activeEngine = d->m_qmlEngine;
    }

    switch (newState) {
    case InferiorRunOk:
        // startup?
        if (d->m_qmlEngine->state() == DebuggerNotReady) {
            setupSlaveEngine();
        } else
        if (d->m_cppEngine->state() == DebuggerNotReady) {
            setupEngine();
        } else
        if (state() == EngineSetupRequested) {
            notifyEngineSetupOk();
        } else
        // breakpoint?
        if (state() == InferiorStopOk) {
            continueInferior();
        } else
        if (state() == InferiorStopRequested) {
            checkErrorState(InferiorStopFailed);
        } else
        if (state() == InferiorRunRequested && (!checkErrorState(InferiorRunFailed)) && (!checkErrorState(InferiorUnrunnable))) {
            notifyInferiorRunOk();
        }
        break;

    case InferiorRunRequested:
        // follow the inferior
        if (state() == InferiorStopOk && checkErrorState(InferiorRunOk)) {
            continueInferior();
        }
        break;

    case InferiorStopRequested:
        // follow the inferior
        if (state() == InferiorRunOk && checkErrorState(InferiorRunOk)) {
            requestInterruptInferior();
        }
        break;

    case InferiorStopOk:
        // check breakpoints
        if (state() == InferiorRunRequested) {
            checkErrorState(InferiorRunFailed);
        } else
        if (checkErrorState(InferiorRunOk)) {
            if (state() == InferiorRunOk) {
                requestInterruptInferior();
            } else
            if (state() == InferiorStopRequested) {
                interruptInferior();
            }
        }
        break;

    case EngineRunFailed:
    case EngineSetupFailed:
    case EngineShutdownFailed:
    case InferiorSetupFailed:
    case InferiorRunFailed:
    case InferiorUnrunnable:
    case InferiorStopFailed:
    case InferiorShutdownFailed:
        if (d->m_errorState == InferiorRunOk) {
            d->m_errorState = newState;
        }
        break;

    case InferiorShutdownRequested:
        break;

    case EngineShutdownRequested:
        // we have to abort the setup before the sub-engines die
        // because we depend on an active runcontrol that will be shut down by the dying engine
        if (state() == EngineSetupRequested)
            notifyEngineSetupFailed();
        break;

    case DebuggerFinished:
        initEngineShutdown();
        break;

    default:
        break;
    }
}

void QmlCppEngine::handleRemoteSetupDone(int gdbServerPort, int qmlPort)
{
    d->m_qmlEngine->handleRemoteSetupDone(gdbServerPort, qmlPort);
    d->m_cppEngine->handleRemoteSetupDone(gdbServerPort, qmlPort);
}

void QmlCppEngine::handleRemoteSetupFailed(const QString &message)
{
    d->m_qmlEngine->handleRemoteSetupFailed(message);
    d->m_cppEngine->handleRemoteSetupFailed(message);
}

DebuggerEngine *QmlCppEngine::cppEngine() const
{
    return d->m_cppEngine;
}

} // namespace Internal
} // namespace Debugger
