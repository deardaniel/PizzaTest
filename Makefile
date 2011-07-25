MAKE = make

default:
	@echo "targets:"
	@echo "All"
	@echo 
	@echo "Libraries"
	@echo " - ATKLib"
	@echo " - HTKLib"
	@echo	
	@echo "TTS"
	@echo " - CMU_US_KAL16"
	@echo " - SYNLib"
	@echo " - US_English"
	@echo " - CMU_Lexicon"
	@echo
	@echo "ATKTests"
	@echo " - TBase"
	@echo " - TSource"
	@echo " - TCode"
	@echo " - TRec"
	@echo " - TIO"
	@echo " - TSyn"
	@echo 
	@echo "ATKApps"
	@echo " - ssds"
	@echo " - asds"
	@echo " - avite"
	@echo 
	@echo "clean"
.PHONY : clean HTKLib ATKLib ATKApps SYNLib CMU_US_KAL16 CMU_Lexicon US_English 
All:	ATKApps ATKTests

HTKLib:	
	echo "Making HTKLib"; 
	if !($(MAKE) -C ./HTKLib); then \
		exit 1; \
	fi

SYNLib:
	echo "Making HTKLib";
	if !($(MAKE) -C ./SYNLib); then \
		exit 1; \
	fi

CMU_US_KAL16:
	echo "Making HTKLib";
	if !($(MAKE) -C ./CMU_US_KAL16); then \
		exit 1; \
	fi

CMU_Lexicon:
	echo "Making HTKLib";
	if !($(MAKE) -C ./CMU_Lexicon); then \
		exit 1; \
	fi

US_English:
	echo "Making HTKLib";
	if !($(MAKE) -C ./US_English); then \
		exit 1; \
	fi

TTS:	SYNLib CMU_US_KAL16 CMU_Lexicon US_English

ATKLib: HTKLib TTS
	echo "Making ATKLib"; \
	if !($(MAKE) -C ./ATKLib); then \
		exit 1; \
	fi 

TBase:	
	if !($(MAKE) -C ./ATKLib TBase); then \
		exit 1; \
	fi
TSource:
	if !($(MAKE) -C ./ATKLib TSource); then \
		exit 1; \
	fi
TCode:
	if !($(MAKE) -C ./ATKLib TCode); then \
		exit 1; \
	fi

TRec:
	if !($(MAKE) -C ./ATKLib TRec); then \
		exit 1; \
	fi

TIO:
	if !($(MAKE) -C ./ATKLib TIO); then \
		exit 1; \
	fi

TSyn:
	if !($(MAKE) -C ./ATKLib TSyn); then \
		exit 1; \
	fi

ATKTests:	ATKLib HTKLib SYNLib CMU_US_KAL16 \
		CMU_Lexicon US_English TIO TBase TSource TSyn TRec TCode

Libraries:	HTKLib ATKLib

avite:	ATKLib
	echo "Making AVite"; \
	if !($(MAKE) -C ./ATKApps/avite); then \
		exit 1; \
	fi

ssds:	ATKLib
	echo "Making SSDS"; \
	if !($(MAKE) -C ./ATKApps/ssds); then \
		exit 1; \
	fi

asds:	ATKLib
	echo "Making ASDS"; \
	if !($(MAKE) -C ./ATKApps/asds); then \
		exit 1; \
	fi

ATKApps:	ssds asds avite

clean:	
	echo "Cleaning everything";\
	if !(cd ./HTKLib && make clean); then \
	    exit 1; \
	fi 
	if !(cd ./ATKLib && make clean); then \
	    exit 1; \
	fi 
	if !(cd ./SYNLib && make clean); then \
	    exit 1; \
	fi 
	if !(cd ./CMU_Lexicon && make clean); then \
	    exit 1; \
	fi 
	if !(cd ./CMU_US_KAL16 && make clean); then \
	    exit 1; \
	fi 
	if !(cd ./US_English && make clean); then \
	    exit 1; \
	fi
	if !(cd ./ATKApps/ssds && make clean); then \
	    exit 1; \
	fi
	if !(cd ./ATKApps/avite && make clean); then \
	    exit 1; \
	fi
	if !(cd ./ATKApps/asds && make clean); then \
            exit 1; \
        fi


