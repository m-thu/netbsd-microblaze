#	$NetBSD: Makefile,v 1.1.26.1 2015/06/04 20:05:46 snj Exp $

LIB=	clangStaticAnalyzerFrontend

.include <bsd.init.mk>

.PATH: ${CLANG_SRCDIR}/lib/StaticAnalyzer/Frontend

SRCS+=	AnalysisConsumer.cpp \
	CheckerRegistration.cpp \
	FrontendActions.cpp \
	ModelConsumer.cpp \
	ModelInjector.cpp

CHECKERS_OBJDIR!=	cd ${.CURDIR}/../libclangStaticAnalyzerCheckers && ${PRINTOBJDIR}
CPPFLAGS+=	-I${CHECKERS_OBJDIR}

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
