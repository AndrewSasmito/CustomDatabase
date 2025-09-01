#!/usr/bin/env python3
"""
Simple script to start the FastAPI server
"""
import uvicorn

if __name__ == "__main__":
    print("Starting FastAPI Database Server (Demo Mode)")
    print("API Documentation: http://localhost:8000/docs")
    print("Health Check: http://localhost:8000/health")
    print("Populate Demo Data: http://localhost:8000/demo/populate")
    print("=" * 60)
    
    uvicorn.run(
        "simple_api_server:app",
        host="127.0.0.1",
        port=8000,
        reload=False,
        log_level="info"
    )
